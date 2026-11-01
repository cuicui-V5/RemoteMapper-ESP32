import argparse
import asyncio
import json
import os
import secrets
import sys
from pathlib import Path
from uuid import uuid4

import adbutils
import uiautomator2 as u2
import uvicorn

from android_agent import Decision, PhoneClient
from android_tools import create_app


def decision(operation, **values):
    return Decision(operation=operation, node_id=values.get("node_id"), package=values.get("package"), text=values.get("text"), key=values.get("key"), explanation="Android 模拟器接口验证")


async def check(args):
    phone = PhoneClient(args.state_dir)
    try:
        await phone.request("POST", "/v1/connect", {"serial": args.serial, "allowed_packages": ["com.android.settings"], "screen_off": True})
        device = await asyncio.to_thread(u2.connect, args.serial)
        before = await phone.observe()
        await phone.execute(decision("app_start", package="com.android.settings"), before, ["com.android.settings"])
        assert await asyncio.to_thread(device(packageName="com.android.settings").wait, timeout=10)
        for page in range(5):
            before = await phone.observe()
            print(json.dumps({"settings_page": page, "labels": [node["text"] for node in before["nodes"] if node["text"]]}, ensure_ascii=False), flush=True)
            titles = [node for node in before["nodes"] if node["resource_id"] == "android:id/title"]
            target = [node for node in titles if node["text"].startswith("Display")]
            if target:
                break
            scrolls = [node for node in before["nodes"] if node["scrollable"] and node["resource_id"] == "com.android.settings:id/recycler_view"]
            assert len(scrolls) == 1, before
            result = await phone.execute(decision("scroll", node_id=scrolls[0]["id"], key="down"), before, ["com.android.settings"])
            assert result["moved"], before
        else:
            raise AssertionError("设置列表中没有找到 Display")
        assert before["nodes"]
        assert all(node["package"] == "com.android.settings" for node in before["nodes"])
        titles = [node for node in before["nodes"] if node["resource_id"] == "android:id/title"]
        assert len(titles) > 1
        target = [node for node in titles if node["text"].startswith("Display")]
        assert len(target) == 1
        await phone.execute(decision("click", node_id=target[0]["id"]), before, ["com.android.settings"])
        assert await asyncio.to_thread(device(textContains="Brightness").wait, timeout=10)
        before = await phone.observe()
        scrolls = [node for node in before["nodes"] if node["scrollable"] and node["resource_id"] == "com.android.settings:id/recycler_view"]
        assert len(scrolls) == 1
        result = await phone.execute(decision("scroll", node_id=scrolls[0]["id"], key="down"), before, ["com.android.settings"])
        assert result["state"] == "complete"
        before = await phone.observe()
        assert all(node["package"] == "com.android.settings" for node in before["nodes"])
        denied = await phone.client.post("/v1/actions", json={"operation_id": str(uuid4()), "generation": before["generation"], "observation_id": before["observation_id"], "operation": "app_start", "package": "com.android.systemui"})
        assert denied.status_code == 403
        for key in ("recent", "volume_up", "volume_down", "play_pause"):
            denied = await phone.client.post("/v1/actions", json={"operation_id": str(uuid4()), "generation": before["generation"], "observation_id": before["observation_id"], "operation": "press", "key": key})
            assert denied.status_code == 403, (key, denied.text)
        await phone.execute(decision("press", key="back"), before, ["com.android.settings"])
        before = await phone.observe()
        await phone.execute(decision("press", key="home"), before, ["com.android.settings"])
        before = await phone.observe()
        assert before["app"]["package"] != "com.android.settings"
        assert before["nodes"] == []
        await phone.execute(decision("app_start", package="com.android.settings"), before, ["com.android.settings"])
        adb_device = adbutils.adb.device(serial=args.serial)
        name_before = await asyncio.to_thread(adb_device.shell2, ["settings", "get", "global", "device_name"], timeout=5)
        assert name_before.returncode == 0
        opened = await asyncio.to_thread(adb_device.shell2, ["am", "start", "-W", "-a", "android.settings.DEVICE_INFO_SETTINGS"], timeout=10)
        assert opened.returncode == 0, opened.output
        assert await asyncio.to_thread(device(text="Device name").wait, timeout=10)
        before = await phone.observe()
        target = [node for node in before["nodes"] if node["text"] == "Device name" and node["resource_id"] == "android:id/title"]
        assert len(target) == 1, before
        await phone.execute(decision("click", node_id=target[0]["id"]), before, ["com.android.settings"])
        assert await asyncio.to_thread(device(className="android.widget.EditText").wait, timeout=10)
        for value in ("RemoteMapper emulator", "", "RemoteMapper input check"):
            before = await phone.observe()
            editors = [node for node in before["nodes"] if node["class_name"] == "android.widget.EditText" and node["resource_id"]]
            assert len(editors) == 1, before
            result = await phone.execute(decision("set_text", node_id=editors[0]["id"], text=value), before, ["com.android.settings"])
            assert result["text_verified"], result
        before = await phone.observe()
        await phone.execute(decision("press", key="back"), before, ["com.android.settings"])
        name_after = await asyncio.to_thread(adb_device.shell2, ["settings", "get", "global", "device_name"], timeout=5)
        assert name_after.returncode == 0 and name_after.output == name_before.output
        power = await asyncio.to_thread(adb_device.shell2, ["dumpsys", "display"], timeout=5)
        assert power.returncode == 0
        assert "mState=OFF" in power.output and "mCommittedState=OFF" in power.output
        pin = await asyncio.to_thread(adb_device.shell2, ["locksettings", "set-pin", "486217"], timeout=5)
        assert pin.returncode == 0, pin.output
        sleep = await asyncio.to_thread(adb_device.shell2, ["input", "keyevent", "KEYCODE_SLEEP"], timeout=5)
        assert sleep.returncode == 0
        locked = await phone.client.get("/v1/observe")
        assert locked.status_code == 423, locked.text
        reconnect = await phone.client.post("/v1/connect", json={"serial": args.serial, "allowed_packages": ["com.android.settings"], "screen_off": True})
        assert reconnect.status_code == 423, reconnect.text
        print(json.dumps({"same_resource_id_selection": "passed", "scroll": "passed", "replace_and_clear_text": "passed", "package_filter": "passed", "unauthorized_app": "rejected", "launcher_content": "empty", "screen_off_operations": "passed", "quiet_keys": "rejected", "locked_observation": "rejected", "locked_connection": "rejected"}, ensure_ascii=False), flush=True)
    finally:
        try:
            await phone.request("POST", "/v1/disconnect", {})
        finally:
            await phone.client.aclose()


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--state-dir", required=True, type=Path)
    args = parser.parse_args()
    adb_device = adbutils.adb.device(serial=args.serial)
    if adb_device.getprop("ro.kernel.qemu") != "1":
        raise RuntimeError("此验证脚本只接受 Android 模拟器")
    os.umask(0o077)
    args.state_dir.mkdir(parents=True, exist_ok=True)
    (args.state_dir / "token").write_text(secrets.token_urlsafe(32) + "\n")
    server = uvicorn.Server(uvicorn.Config(create_app(args.state_dir), host="127.0.0.1", port=8765, access_log=False))
    async with asyncio.TaskGroup() as group:
        serving = group.create_task(server.serve())
        while not server.started:
            if serving.done():
                serving.result()
                raise RuntimeError("Android 工具服务启动失败")
            await asyncio.sleep(0.05)
        try:
            directory = Path(__file__).parent
            for command in (
                [sys.executable, str(directory / "check_service.py"), "--state-dir", str(args.state_dir)],
                [sys.executable, str(directory / "check_reliability.py"), "--state-dir", str(args.state_dir), "--adb", adbutils.adb_path(), "--serial", args.serial, "--output", str(args.state_dir / "reliability.json")],
            ):
                process = await asyncio.create_subprocess_exec(*command)
                assert await process.wait() == 0, command
            await check(args)
        finally:
            server.should_exit = True
            await serving


if __name__ == "__main__":
    asyncio.run(main())
