import argparse
import asyncio
import json
from pathlib import Path
from unittest import TestCase

from serial_link import SerialLink


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    link = SerialLink(args.port, args.work_dir)
    check = TestCase()
    evidence = []
    try:
        initial = await link.start()
        check.assertEqual(initial["mode"], "mac")
        state = await link.agent("mode", mode="agent")
        check.assertEqual(state["mode"], "agent")
        duplicate = {"op": "mode", "session": link.session, "command": link.command, "mode": "agent"}
        replay = await link.raw("agent " + json.dumps(duplicate, ensure_ascii=False, separators=(",", ":")))
        check.assertEqual(replay, state)
        state = await link.agent("submit", text="检查实体确认")
        check.assertEqual(state["stage"], "confirm_task")
        with check.assertRaisesRegex(RuntimeError, "invalid_transition"):
            await link.agent("confirm", task=state["task"], step=state["step"])
        check.assertEqual((await link.agent("heartbeat"))["stage"], "confirm_task")
        state = await link.agent("cancel")
        check.assertEqual(state["stage"], "cancelled")
        evidence.append({"request_replay": "passed", "physical_confirmation_required": "passed", "host_cancel": "passed"})
        await asyncio.sleep(5.3)
        state = await link.raw("agent")
        check.assertEqual(state["mode"], "mac")
        check.assertFalse(state["attached"])
        check.assertEqual(state["stage"], "cancelled")
        evidence.append({"host_lease_expiry": "passed", "state": state})
        (args.work_dir / "firmware-check.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
        print(json.dumps(evidence, ensure_ascii=False, indent=2))
    finally:
        link.close()


if __name__ == "__main__":
    asyncio.run(main())
