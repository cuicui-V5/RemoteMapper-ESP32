import argparse
import asyncio
from pathlib import Path

import uvicorn

import bridge
from android_tools import create_app


async def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--state-dir", required=True, type=Path)
    args, _ = parser.parse_known_args()
    server = uvicorn.Server(uvicorn.Config(create_app(args.state_dir), host="127.0.0.1", port=8765, access_log=False))
    async with asyncio.TaskGroup() as group:
        task = group.create_task(server.serve())
        while not server.started:
            if task.done():
                task.result()
                raise RuntimeError("Android 工具服务启动失败")
            await asyncio.sleep(0.05)
        try:
            await bridge.main()
        finally:
            server.should_exit = True


if __name__ == "__main__":
    asyncio.run(main())
