import asyncio
import json
import secrets
import sqlite3
import time
from contextlib import asynccontextmanager
from pathlib import Path

import serial


class SerialLink:
    def __init__(self, port: str, directory: Path):
        self.serial = serial.Serial(port, 115200, timeout=2, write_timeout=2, exclusive=True)
        self.lock = asyncio.Lock()
        self.session = secrets.token_hex(16)
        self.command = 0
        self.directory = directory
        directory.mkdir(parents=True, exist_ok=True)
        self.database = sqlite3.connect(directory / "remote-events.sqlite3")
        self.database.execute("CREATE TABLE IF NOT EXISTS events(boot TEXT, stream INTEGER, sequence INTEGER, payload TEXT NOT NULL, PRIMARY KEY(boot,stream,sequence))")

    def exchange(self, command):
        data = (command + "\n").encode()
        if len(data) > 8192:
            raise ValueError("串口命令超过长度上限")
        self.serial.write(data)
        self.serial.flush()
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            line = self.serial.read_until(b"\n", 16384)
            if not line:
                continue
            if not line.endswith(b"\n"):
                raise RuntimeError("串口返回的消息不完整")
            decoded = line.decode().strip()
            if not decoded.startswith("{"):
                with (self.directory / "device.log").open("a") as log:
                    log.write(decoded + "\n")
                continue
            response = json.loads(decoded)
            if "error" in response:
                raise RuntimeError("ESP32：" + str(response["error"]))
            return response
        raise TimeoutError("ESP32 串口响应超时")

    async def raw(self, command):
        async with self.lock:
            return await asyncio.to_thread(self.exchange, command)

    async def agent(self, operation, **values):
        async with self.lock:
            self.command += 1
            request = {"op": operation, "session": self.session, "command": self.command, **values}
            return await asyncio.to_thread(self.exchange, "agent " + json.dumps(request, ensure_ascii=False, separators=(",", ":")))

    async def start(self):
        status = await self.raw("status")
        if status["version"] != "1.2.1-macos.12":
            raise RuntimeError("需要 1.2.1-macos.12 固件")
        attached = await self.raw("agent " + json.dumps({"op": "attach", "session": self.session}, separators=(",", ":")))
        first = await self.raw('events {"begin":true}')
        self.boot = first["boot_id"]
        self.stream = first["stream_id"]
        self.acknowledged = first["acknowledged"]
        self.persist(first)
        return attached

    @asynccontextmanager
    async def keep_alive(self):
        finished = asyncio.Event()
        async def heartbeat():
            while not finished.is_set():
                await self.agent("heartbeat")
                await asyncio.sleep(0.5)
        async with asyncio.TaskGroup() as group:
            task = group.create_task(heartbeat())
            try:
                yield
            finally:
                finished.set()
                await task

    def persist(self, batch):
        if batch["boot_id"] != self.boot or batch["stream_id"] != self.stream or batch["dropped"]:
            raise RuntimeError("ESP32 重启、事件流变化或事件丢失，任务已停止")
        for event in batch["events"]:
            if event["sequence"] != self.acknowledged + 1:
                raise RuntimeError("遥控器事件序号不连续")
            self.database.execute("INSERT INTO events VALUES (?,?,?,?)", (self.boot, self.stream, event["sequence"], json.dumps(event, separators=(",", ":"))))
            self.acknowledged = event["sequence"]
        self.database.commit()

    async def events(self):
        request = {"boot_id": self.boot, "stream_id": self.stream, "ack": self.acknowledged}
        batch = await self.raw("events " + json.dumps(request, separators=(",", ":")))
        self.persist(batch)
        return batch

    def close(self):
        self.serial.close()
        self.database.close()
