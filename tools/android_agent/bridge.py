import argparse
import asyncio
import json
import os
import signal
import sys
import time
import wave
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from uuid import uuid4

import sounddevice as sd

from android_agent import Decision, INSTRUCTION, PhoneClient
from asr import transcribe
from codex_model import CodexModel
from serial_link import SerialLink


class Announcer:
    def __init__(self):
        self.process = None

    async def stop(self):
        if self.process is not None:
            if self.process.returncode is None:
                self.process.terminate()
            await self.process.wait()
            self.process = None

    async def speak(self, text):
        await self.stop()
        self.process = await asyncio.create_subprocess_exec("/usr/bin/say", "-v", "Tingting", text)

    async def state(self, state, previous_mode):
        mode, stage = state["mode"], state["stage"]
        if stage == "listening":
            await self.stop()
        elif stage == "transcribing":
            await self.speak("正在识别语音，请稍候。")
        elif stage == "model" and state["step"] == 0:
            await self.speak("正在读取手机界面并处理任务。")
        elif stage == "confirm_task":
            await self.speak("识别结果：" + state["text"] + "。请按方向圆环中央的确认键。按返回键取消。")
        elif stage == "confirm_action":
            await self.speak(state["action"]["explanation"] + "。请按中央确认键执行。按返回键取消。")
        elif stage == "done" and state["text"]:
            await self.speak("任务完成。" + state["detail"])
        elif stage == "cancelled":
            await self.speak("任务已取消。")
        elif stage == "error":
            await self.speak("任务已停止。请查看程序中的错误信息。")
        elif mode != previous_mode:
            messages = {"mac": "Mac 模式。语音键使用微信输入法。", "android": "Android 模式。方向、确认和返回按键控制手机。", "agent": "Agent 模式。按住麦克风图标按键讲话，松开后等待语音提示。"}
            await self.speak(messages[mode])


class Recorder:
    def __init__(self, directory):
        self.directory = directory / "audio" / uuid4().hex
        self.directory.mkdir(parents=True, exist_ok=True)
        self.stream = None
        devices = [index for index, device in enumerate(sd.query_devices()) if device["name"] == "RemoteMapper Wireless Microphone" and device["max_input_channels"] > 0]
        if len(devices) != 1:
            raise RuntimeError("需要连接唯一的 RemoteMapper 麦克风")
        self.device = devices[0]

    async def start(self, task):
        self.task = task
        self.audio = self.directory / f"voice-{task}.wav"
        if self.audio.exists():
            raise FileExistsError(self.audio)
        self.frames = []
        self.frame_count = 0
        self.error = None

        def receive(data, frames, timing, status):
            if status or self.frame_count + frames > 16000 * 90:
                self.error = str(status) if status else "录音超过 90 秒"
                raise sd.CallbackAbort
            self.frames.append(bytes(data))
            self.frame_count += frames

        started = time.monotonic()
        self.stream = sd.RawInputStream(device=self.device, samplerate=16000, channels=1, dtype="int16", blocksize=512, latency="low", callback=receive)
        await asyncio.to_thread(self.stream.start)
        (self.directory / f"voice-{task}-capture.json").write_text(json.dumps({"start_seconds": time.monotonic() - started, "device": sd.query_devices(self.device)}, ensure_ascii=False, indent=2))

    def check(self):
        if self.error is not None:
            raise RuntimeError("麦克风录音失败：" + self.error)

    async def stop(self):
        if self.stream is None:
            raise RuntimeError("录音尚未启动")
        await asyncio.to_thread(self.stream.stop)
        self.stream.close()
        self.stream = None
        self.check()
        with wave.open(str(self.audio), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(16000)
            output.writeframes(b"".join(self.frames))
        return self.audio


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--state-dir", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--asr-model", required=True, type=Path)
    parser.add_argument("--model", default="gpt-6-astra")
    parser.add_argument("--mode", choices=["mac", "android", "agent"], default="mac")
    parser.add_argument("--allow-package", action="append", required=True)
    task_input = parser.add_mutually_exclusive_group()
    task_input.add_argument("--task")
    task_input.add_argument("--transcript-file", type=Path)
    parser.add_argument("--exit-after-task", action="store_true")
    args = parser.parse_args()
    if args.transcript_file is not None:
        transcript = json.loads(args.transcript_file.read_text())
        args.task = transcript["text"]
        if not isinstance(args.task, str) or not args.task.strip():
            raise ValueError("语音识别文件没有有效文字")
    os.umask(0o077)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    recorder = Recorder(args.work_dir)
    phone = PhoneClient(args.state_dir)
    link = SerialLink(args.port, args.work_dir)
    announcer = Announcer()
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    loop.add_signal_handler(signal.SIGINT, stop.set)
    loop.add_signal_handler(signal.SIGTERM, stop.set)
    asr_executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix="asr")
    history = []
    before = None
    active_task = None
    job = None
    job_kind = None
    seen_state = None
    previous_mode = None
    terminal_tasks = set()
    phone_connected = False
    phone_ready = False
    phone_generation = None
    link_attached = False
    evidence = (args.work_dir / "tasks.jsonl").open("a", buffering=1)

    def record(kind, value):
        evidence.write(json.dumps({"kind": kind, "value": value}, ensure_ascii=False) + "\n")
        if kind in ("state", "transcript", "decision", "result"):
            print(json.dumps({"kind": kind, "value": value}, ensure_ascii=False), flush=True)

    async def ensure_phone():
        nonlocal phone_connected, phone_ready, phone_generation
        if not phone_ready:
            phone_connected = True
            connected = await phone.request("POST", "/v1/connect", {"serial": args.serial, "allowed_packages": args.allow_package})
            phone_generation = connected["generation"]
            phone_ready = True

    async def model_step(model, state):
        await ensure_phone()
        observed = await phone.observe()
        payload = {"task": state["text"], "allowed_packages": args.allow_package, "history": history, "observation": observed}
        result = Decision.model_validate(await model.decide(INSTRUCTION, payload, Decision.model_json_schema()))
        return observed, result

    async def execute_step(state):
        await ensure_phone()
        async def authorize():
            current = await link.agent("heartbeat")
            if (current["task"], current["step"], current["stage"]) != (state["task"], state["step"], "tool"):
                raise RuntimeError("实体遥控器已经取消或接管任务")
        action = state["action"]
        if action.get("manual"):
            decision = Decision(operation="press", node_id=None, package=None, text=None, key=action["key"], explanation="实体按键操作")
            await authorize()
            result = await phone.request("POST", "/v1/actions", {"operation_id": str(uuid4()), "generation": phone_generation, "observation_id": None, "operation": "press", "key": decision.key})
            return {"decision": decision.model_dump(), "result": result}
        else:
            decision = Decision.model_validate(action)
            observed = before
        result = await phone.execute(decision, observed, args.allow_package, authorize=authorize)
        after = await phone.observe()
        return {"decision": decision.model_dump(), "result": result, "after": after}

    try:
        await link.start()
        link_attached = True
        await link.agent("mode", mode=args.mode)
        if args.task:
            await link.agent("submit", text=args.task)
        async with link.keep_alive(), CodexModel(args.model, args.work_dir / "model") as model:
            while not stop.is_set():
                state = await link.agent("heartbeat")
                batch = await link.events()
                if state["mode"] == "agent" and any(event["key"] == 102 and event["pressed"] for event in batch["events"]):
                    await announcer.speak("语音请按遥控器上带麦克风图标的按键。")
                signature = (state["mode"], state["task"], state["step"], state["stage"])
                if signature != seen_state:
                    record("state", state)
                    seen_state = signature
                    await announcer.state(state, previous_mode)
                    previous_mode = state["mode"]
                if recorder.stream is not None:
                    recorder.check()
                if state["stage"] == "listening" and recorder.stream is None:
                    await recorder.start(state["task"])
                if active_task != state["task"] or (state["stage"] in ("cancelled", "error") and state["task"] not in terminal_tasks):
                    had_job = job is not None
                    if job is not None:
                        if job_kind == "model":
                            await model.interrupt()
                        job.cancel()
                        await asyncio.wait([job])
                        if not job.cancelled():
                            job.result()
                        job = None
                    if recorder.stream is not None and (recorder.task != state["task"] or state["stage"] in ("cancelled", "error")):
                        await recorder.stop()
                    if phone_connected and (had_job or state["stage"] in ("cancelled", "error")):
                        await phone.request("POST", "/v1/cancel", {})
                        phone_ready = False
                    active_task = state["task"]
                    history = []
                    before = None
                if state["mode"] == "mac" and phone_connected:
                    await phone.request("POST", "/v1/disconnect", {})
                    phone_connected = False
                    phone_ready = False
                if job is not None and job.done():
                    value = job.result()
                    job = None
                    if job_kind == "model":
                        before, decision = value
                        record("observation", before)
                        record("decision", decision.model_dump())
                        await link.agent("decision", task=state["task"], step=state["step"], action=decision.model_dump())
                    elif job_kind == "asr":
                        record("transcript", value)
                        await link.agent("submit", task=state["task"], text=value["text"])
                    elif job_kind == "tool":
                        record("result", value)
                        history.append({"decision": value["decision"], "result": value["result"]})
                        await link.agent("result", task=state["task"], step=state["step"], ok=True)
                    continue
                if job is None:
                    if state["stage"] == "listening" and recorder.stream is None:
                        await recorder.start(state["task"])
                    elif state["stage"] == "transcribing":
                        audio = await recorder.stop()
                        job_kind = "asr"
                        job = asyncio.get_running_loop().run_in_executor(asr_executor, transcribe, audio, args.asr_model)
                    elif state["stage"] == "model":
                        job_kind = "model"
                        job = asyncio.create_task(model_step(model, state))
                    elif state["stage"] == "tool":
                        job_kind = "tool"
                        job = asyncio.create_task(execute_step(state))
                if state["stage"] in ("done", "cancelled", "error") and state["task"] not in terminal_tasks:
                    terminal_tasks.add(state["task"])
                    if args.exit_after_task:
                        return
                await asyncio.sleep(0.1)
            if job is not None:
                if job_kind == "model":
                    await model.interrupt()
                if not job.done():
                    job.cancel()
                    await asyncio.wait([job])
                if not job.cancelled():
                    job.result()
                job = None
    finally:
        failure = sys.exception()
        try:
            if recorder.stream is not None:
                await recorder.stop()
            if job is not None and not job.done():
                job.cancel()
                await asyncio.wait([job])
        finally:
            try:
                if link_attached:
                    await link.agent("cancel")
                    await link.agent("mode", mode="mac")
            finally:
                link.close()
                try:
                    if phone_connected:
                        await phone.request("POST", "/v1/cancel", {})
                        await phone.request("POST", "/v1/disconnect", {})
                finally:
                    await phone.client.aclose()
                    evidence.close()
                    asr_executor.shutdown(wait=True, cancel_futures=True)
                    if failure is not None:
                        await announcer.speak("Agent 已停止。错误详情已显示在程序中。")
                    if announcer.process is not None:
                        await announcer.process.wait()


if __name__ == "__main__":
    asyncio.run(main())
