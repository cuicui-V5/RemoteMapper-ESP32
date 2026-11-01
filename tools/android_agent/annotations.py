import argparse
import asyncio
import json
import sqlite3
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

from android_agent import PhoneClient


def save(directory: Path, observation: dict, note: str, kind: str, position_resource_id: str | None):
    position = None
    if position_resource_id:
        nodes = [node for node in observation["nodes"] if node["resource_id"] == position_resource_id]
        if len(nodes) != 1 or not nodes[0]["text"]:
            raise ValueError("阅读位置或媒体时间需要由当前界面中的唯一文字控件提供")
        position = {"resource_id": position_resource_id, "text": nodes[0]["text"]}
    if kind in ("reading", "media", "course") and position is None:
        raise ValueError("需要指定显示阅读位置或播放时间的控件")
    record = {"id": str(uuid4()), "created_at": datetime.now(timezone.utc).isoformat(), "kind": kind, "note": note, "position": position, "observation": observation}
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    with sqlite3.connect(directory / "annotations.sqlite3") as database:
        database.execute("CREATE TABLE IF NOT EXISTS annotations (id TEXT PRIMARY KEY, created TEXT NOT NULL, payload TEXT NOT NULL)")
        database.execute("INSERT INTO annotations VALUES (?,?,?)", (record["id"], record["created_at"], json.dumps(record, ensure_ascii=False)))
    output = directory / (record["id"] + ".json")
    output.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n")
    return record, output


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--note", required=True)
    parser.add_argument("--kind", choices=["screen", "reading", "media", "course"], default="screen")
    parser.add_argument("--position-resource-id")
    args = parser.parse_args()
    phone = PhoneClient(args.state_dir)
    try:
        observation = await phone.observe()
        record, output = save(args.directory, observation, args.note, args.kind, args.position_resource_id)
        print(json.dumps({"id": record["id"], "path": str(output.resolve()), "app": observation["app"], "position": record["position"]}, ensure_ascii=False))
    finally:
        await phone.client.aclose()


if __name__ == "__main__":
    asyncio.run(main())
