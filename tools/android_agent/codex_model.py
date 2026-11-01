import argparse
import asyncio
import json
import re
import tomllib
from pathlib import Path

from pydantic import BaseModel, ConfigDict


class ModelReply(BaseModel):
    model_config = ConfigDict(extra="forbid")
    text: str


class CodexModel:
    def __init__(self, model: str, work_dir: Path):
        self.model = model
        self.work_dir = work_dir.resolve()
        self.process = None
        self.sequence = 0
        self.pending = {}
        self.events = asyncio.Queue()

    async def __aenter__(self):
        self.work_dir.mkdir(parents=True, exist_ok=True)
        self.error_log = (self.work_dir / "codex.log").open("a")
        arguments = ["codex", "app-server", "-c", 'model_provider="openai"', "-c", 'web_search="disabled"']
        config_path = Path.home() / ".codex" / "config.toml"
        if config_path.is_file():
            config = tomllib.loads(config_path.read_text())
            for name in config.get("mcp_servers", {}):
                if re.fullmatch(r"[A-Za-z0-9_-]+", name) is None:
                    raise ValueError("MCP 配置名称不能安全用于命令行设置")
                arguments.extend(["-c", "mcp_servers." + name + ".enabled=false"])
        for feature in ("shell_tool", "apps", "plugins", "hooks", "memories", "multi_agent", "browser_use", "computer_use", "view_image", "image_generation"):
            arguments.extend(["--disable", feature])
        self.process = await asyncio.create_subprocess_exec(*arguments, stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=self.error_log, cwd=self.work_dir, limit=4 * 1024 * 1024)
        self.reader = asyncio.create_task(self.read_messages())
        await self.request("initialize", {"clientInfo": {"name": "remotemapper_model", "title": "RemoteMapper Model", "version": "1.0.0"}, "capabilities": {"experimentalApi": True}})
        await self.send({"method": "initialized", "params": {}})
        return self

    async def send(self, message):
        self.process.stdin.write((json.dumps(message, ensure_ascii=False) + "\n").encode())
        await self.process.stdin.drain()

    async def read_messages(self):
        try:
            while line := await self.process.stdout.readline():
                message = json.loads(line)
                if "id" in message and "method" not in message:
                    future = self.pending.pop(message["id"])
                    if future.done():
                        continue
                    if "error" in message:
                        future.set_exception(RuntimeError(json.dumps(message["error"], ensure_ascii=False)))
                    else:
                        future.set_result(message["result"])
                else:
                    await self.events.put(message)
        finally:
            for future in self.pending.values():
                if not future.done():
                    future.set_exception(RuntimeError("Codex App Server 连接已结束"))
            await self.events.put({"method": "connection_closed"})

    async def request(self, method, params):
        self.sequence += 1
        future = asyncio.get_running_loop().create_future()
        self.pending[self.sequence] = future
        await self.send({"id": self.sequence, "method": method, "params": params})
        return await asyncio.wait_for(future, 120)

    async def decide(self, instruction: str, payload: dict, schema: dict):
        started = await self.request("thread/start", {"model": self.model, "modelProvider": "openai", "ephemeral": True, "cwd": str(self.work_dir), "environments": [], "approvalPolicy": "never", "sandbox": "read-only", "baseInstructions": instruction, "developerInstructions": "仅根据提供的数据返回 JSON。禁止调用内置工具。", "config": {"project_doc_max_bytes": 0}, "dynamicTools": []})
        thread_id = started["thread"]["id"]
        turn = await self.request("turn/start", {"threadId": thread_id, "input": [{"type": "text", "text": json.dumps(payload, ensure_ascii=False)}], "environments": [], "effort": "low", "outputSchema": schema})
        turn_id = turn["turn"]["id"]
        self.active = (thread_id, turn_id)
        final = None
        async with asyncio.timeout(120):
            while True:
                message = await self.events.get()
                method = message.get("method")
                if "id" in message:
                    await self.send({"id": message["id"], "error": {"code": -32601, "message": "本客户端仅接受模型决策"}})
                    raise RuntimeError("模型请求了未开放的工具或授权")
                if method == "connection_closed":
                    raise RuntimeError("Codex App Server 连接已结束")
                params = message.get("params", {})
                if params.get("threadId") != thread_id:
                    continue
                if method == "item/completed":
                    item = params["item"]
                    if item["type"] == "agentMessage":
                        final = item["text"]
                    elif item["type"] not in ("userMessage", "reasoning"):
                        raise RuntimeError("模型执行了额外操作")
                if method == "turn/completed":
                    self.active = None
                    if params["turn"]["status"] != "completed" or not final:
                        raise RuntimeError(json.dumps(params["turn"], ensure_ascii=False))
                    return json.loads(final)

    async def interrupt(self):
        if getattr(self, "active", None):
            thread_id, turn_id = self.active
            await self.request("turn/interrupt", {"threadId": thread_id, "turnId": turn_id})

    async def __aexit__(self, exc_type, exc, traceback):
        if self.process.returncode is None:
            self.process.terminate()
        await self.process.wait()
        await self.reader
        self.error_log.close()


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", default="gpt-6-astra")
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    async with CodexModel(args.model, args.work_dir) as model:
        response = await model.decide("你是模型连接检查程序。按用户要求返回结构化文字。", {"request": "text 字段请填写连接成功"}, ModelReply.model_json_schema())
        assert ModelReply.model_validate(response).text == "连接成功"
        print(json.dumps(response, ensure_ascii=False))


if __name__ == "__main__":
    asyncio.run(main())
