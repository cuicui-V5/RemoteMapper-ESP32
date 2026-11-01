import argparse
import hashlib
import json
import os
import secrets
import sqlite3
import threading
import time
from dataclasses import asdict
from contextlib import asynccontextmanager
from importlib.metadata import version
from pathlib import Path
from typing import Literal
from uuid import UUID, uuid4

import adbutils
import uiautomator2 as u2
import uvicorn
from defusedxml import ElementTree
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, ConfigDict, Field, model_validator

MAX_RESPONSE_BYTES = 12000
MAX_REQUEST_BYTES = 16384
MAX_XML_BYTES = 1024 * 1024


class Connection(BaseModel):
    model_config = ConfigDict(extra="forbid")
    serial: str = Field(min_length=1, max_length=160)


class Action(BaseModel):
    model_config = ConfigDict(extra="forbid")
    operation_id: UUID
    generation: UUID
    observation_id: UUID | None
    operation: Literal["app_start", "click", "set_text", "press"]
    package: str | None = Field(default=None, max_length=200)
    node_id: int | None = Field(default=None, ge=0)
    text: str | None = Field(default=None, max_length=2000)
    key: Literal["back", "home", "up", "down", "left", "right", "enter", "recent", "volume_up", "volume_down", "play_pause"] | None = None

    @model_validator(mode="after")
    def check_arguments(self):
        if self.observation_id is None and self.operation != "press":
            raise ValueError("控件操作需要界面记录")
        expected = {
            "app_start": {"package"},
            "click": {"node_id"},
            "set_text": {"node_id", "text"},
            "press": {"key"},
        }[self.operation]
        present = {name for name in ("package", "node_id", "text", "key") if getattr(self, name) is not None}
        if present != expected:
            raise ValueError("操作参数与 operation 不一致")
        return self


def compact(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


class AndroidTools:
    def __init__(self, state_dir: Path):
        self.lock = threading.Lock()
        self.control_lock = threading.Lock()
        self.generation = uuid4()
        self.device = None
        self.observation = None
        self.nodes = []
        self.cancelled = False
        self.selected_serial = None
        self.monitor_failed = False
        self.gkd_marker = state_dir / "gkd-session.json"
        self.db = sqlite3.connect(state_dir / "operations.sqlite3", check_same_thread=False)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("CREATE TABLE IF NOT EXISTS operations (id TEXT PRIMARY KEY, request TEXT NOT NULL, state TEXT NOT NULL, result TEXT, created REAL NOT NULL)")
        self.db.execute("UPDATE operations SET state='interrupted' WHERE state='running'")
        self.db.commit()
        self.monitor = threading.Thread(target=self.track_connections, name="adb-connections", daemon=True)
        self.monitor.start()

    def track_connections(self):
        try:
            for event in adbutils.adb.track_devices():
                with self.control_lock:
                    if event.serial == self.selected_serial and event.status != "device":
                        self.cancelled = True
                        self.generation = uuid4()
        finally:
            with self.control_lock:
                self.monitor_failed = True
                self.cancelled = True
                self.generation = uuid4()

    def devices(self):
        return [asdict(item) for item in adbutils.adb.list(extended=True)]

    def require_device(self):
        if self.monitor_failed:
            raise HTTPException(503, "ADB 连接监听已经终止，需要重新启动服务")
        if self.device is None:
            raise HTTPException(409, "尚未选择 Android 设备")
        if self.device.adb_device.get_state() != "device":
            raise HTTPException(409, "Android 设备连接不可用")
        return self.device

    def connect(self, serial):
        with self.lock:
            available = {item["serial"]: item["state"] for item in self.devices()}
            if available.get(serial) != "device":
                raise HTTPException(409, "指定设备未连接或尚未授权 USB 调试")
            if self.device is not None:
                self.device.stop_uiautomator()
            self.device = None
            self.observation = None
            with self.control_lock:
                self.generation = uuid4()
                self.cancelled = False
                self.selected_serial = serial
            adb_device = adbutils.adb.device(serial=serial)
            running = adb_device.shell2(["pidof", "li.songe.gkd"], timeout=5)
            if running.returncode == 0:
                self.gkd_marker.write_text(compact({"serial": serial, "restore": True}))
                stopped = adb_device.shell2(["am", "force-stop", "li.songe.gkd"], timeout=5)
                if stopped.returncode != 0:
                    raise RuntimeError("无法暂停 GKD 自动化服务")
            elif running.returncode != 1:
                raise RuntimeError("无法读取 GKD 运行状态")
            device = u2.connect(adb_device)
            device.settings["wait_timeout"] = 2.0
            info = device.info
            properties = {}
            for name in ("ro.product.manufacturer", "ro.product.model", "ro.vendor.oplus.market.name", "ro.build.version.release", "ro.build.version.sdk", "ro.build.version.oplusrom", "ro.build.version.oplusrom.display"):
                result = device.shell(["getprop", name], timeout=5)
                if result.exit_code != 0:
                    raise RuntimeError(f"getprop 执行失败：{name}")
                properties[name] = result.output.strip()
            self.device = device
            return {"generation": str(self.generation), "serial": serial, "properties": properties, "info": info}

    def disconnect(self):
        self.cancel()
        with self.lock:
            if self.device is not None:
                self.device.stop_uiautomator()
            self.device = None
            self.observation = None
            with self.control_lock:
                self.selected_serial = None
            if self.gkd_marker.exists():
                saved = json.loads(self.gkd_marker.read_text())
                adb_device = adbutils.adb.device(serial=saved["serial"])
                restored = adb_device.shell2(["am", "start", "-W", "-n", "li.songe.gkd/.MainActivity"], timeout=15)
                if restored.returncode != 0 or adb_device.shell2(["pidof", "li.songe.gkd"], timeout=5).returncode != 0:
                    raise RuntimeError("GKD 应用恢复失败")
                self.gkd_marker.unlink()
            return {"state": "disconnected"}

    def cancel(self):
        with self.control_lock:
            self.cancelled = True
            self.generation = uuid4()
            return {"state": "cancelled", "generation": str(self.generation), "in_flight": "已经提交给 Android 的单次操作可能仍然完成；后续操作已被禁止"}

    def read_tree(self):
        device = self.require_device()
        app_before = device.app_current()
        xml = device.dump_hierarchy(compressed=False, pretty=False, max_depth=50)
        app_after = device.app_current()
        if app_before != app_after:
            raise HTTPException(409, "读取期间前台应用发生变化，需要重新读取")
        if len(xml.encode("utf-8")) > MAX_XML_BYTES:
            raise HTTPException(413, "Android 界面超过读取上限")
        root = ElementTree.fromstring(xml)
        elements = list(root.iter("node"))
        if not elements:
            raise RuntimeError("Android 返回空界面")
        nodes = []
        for index, element in enumerate(elements):
            attrs = element.attrib
            password = attrs.get("password") == "true"
            node = {
                "id": index,
                "resource_id": attrs.get("resource-id", ""),
                "class_name": attrs.get("class", ""),
                "package": attrs.get("package", ""),
                "text": "" if password else attrs.get("text", ""),
                "description": "" if password else attrs.get("content-desc", ""),
                "bounds": attrs.get("bounds", ""),
                "clickable": attrs.get("clickable") == "true",
                "enabled": attrs.get("enabled") == "true",
                "password": password,
                "focused": attrs.get("focused") == "true",
                "selected": attrs.get("selected") == "true",
                "checked": attrs.get("checked") == "true",
                "scrollable": attrs.get("scrollable") == "true",
            }
            nodes.append(node)
        app_nodes = [{key: value for key, value in node.items() if key != "id"} for node in nodes if node["package"] == app_after["package"]]
        digest = hashlib.sha256(compact({"app": app_after, "nodes": app_nodes}).encode()).hexdigest()
        return nodes, digest, app_after

    def observe(self):
        with self.lock:
            nodes, digest, app = self.read_tree()
            with self.control_lock:
                generation = str(self.generation)
            self.nodes = nodes
            self.observation = {
                "observation_id": str(uuid4()),
                "generation": generation,
                "created_monotonic": time.monotonic(),
                "digest": digest,
                "app": app,
                "total_nodes": len(nodes),
            }
            return self.page_unlocked(0)

    def page_unlocked(self, offset):
        if self.observation is None:
            raise HTTPException(409, "需要先读取当前界面")
        if offset > len(self.nodes):
            raise HTTPException(422, "分页位置超出界面范围")
        result = {key: value for key, value in self.observation.items() if key != "created_monotonic"}
        result.update(nodes=[], next_offset=None)
        for node in self.nodes[offset:]:
            result["nodes"].append(node)
            result["next_offset"] = node["id"] + 1 if node["id"] + 1 < len(self.nodes) else None
            if len(compact(result).encode()) > MAX_RESPONSE_BYTES:
                result["nodes"].pop()
                result["next_offset"] = node["id"]
                if not result["nodes"]:
                    raise HTTPException(413, "单个控件内容超过响应上限")
                break
        return result

    def page(self, observation_id, offset):
        with self.lock:
            if self.observation is None or self.observation["observation_id"] != str(observation_id):
                raise HTTPException(409, "界面记录已经失效")
            return self.page_unlocked(offset)

    def select(self, node_id):
        if node_id >= len(self.nodes):
            raise HTTPException(422, "控件编号超出范围")
        node = self.nodes[node_id]
        if node["package"] != self.observation["app"]["package"]:
            raise HTTPException(409, "控件需要属于当前前台应用")
        if node["password"] or not node["enabled"] or not node["resource_id"]:
            raise HTTPException(409, "需要有 resourceId 的已启用普通控件")
        selector = {"resourceId": node["resource_id"], "className": node["class_name"], "packageName": node["package"]}
        obj = self.device(**selector)
        if obj.count != 1:
            raise HTTPException(409, "resourceId 选择结果不唯一")
        return obj

    def execute(self, action):
        request_text = compact(action.model_dump(mode="json"))
        with self.lock:
            row = self.db.execute("SELECT request,state,result FROM operations WHERE id=?", (str(action.operation_id),)).fetchone()
            if row:
                if row[0] != request_text:
                    raise HTTPException(409, "operation_id 已被其他请求使用")
                if row[1] != "complete":
                    raise HTTPException(409, "操作结果未确定，需要重新读取手机状态")
                return json.loads(row[2])
            with self.control_lock:
                if self.cancelled or action.generation != self.generation:
                    raise HTTPException(409, "操作已取消或连接代次已经变化")
            device = self.require_device()
            if action.observation_id is not None:
                if self.observation is None or str(action.observation_id) != self.observation["observation_id"]:
                    raise HTTPException(409, "操作引用的界面记录已经失效")
                if time.monotonic() - self.observation["created_monotonic"] > 30:
                    raise HTTPException(409, "界面记录超过 30 秒，需要重新读取")
                _, current_digest, _ = self.read_tree()
                if current_digest != self.observation["digest"]:
                    raise HTTPException(409, "手机界面已经变化，需要重新读取")
            obj = self.select(action.node_id) if action.node_id is not None else None
            if action.operation == "app_start" and action.package not in device.app_list():
                raise HTTPException(422, "手机尚未安装指定应用")
            with self.control_lock:
                if self.cancelled or action.generation != self.generation:
                    raise HTTPException(409, "操作已取消")
                self.db.execute("INSERT INTO operations VALUES (?,?, 'running',NULL,?)", (str(action.operation_id), request_text, time.time()))
                self.db.commit()
            completed = False
            try:
                details = {}
                if action.operation == "app_start":
                    resolved = device.shell(["cmd", "package", "resolve-activity", "--brief", "-a", "android.intent.action.MAIN", "-c", "android.intent.category.LAUNCHER", action.package], timeout=5)
                    if resolved.exit_code != 0:
                        raise RuntimeError("读取应用入口失败")
                    component = resolved.output.strip().splitlines()[-1]
                    package, separator, activity = component.partition("/")
                    if separator != "/" or package != action.package or not activity:
                        raise RuntimeError("应用没有可用的启动入口")
                    device.app_start(action.package, activity=activity, wait=True, stop=False)
                    if device.app_current()["package"] != action.package:
                        raise RuntimeError("应用启动后前台包名不符合请求")
                elif action.operation == "click":
                    # 控件位置由 uiautomator2 查询，ADB 每次提交一次点击。
                    info = obj.info
                    bounds = info["visibleBounds"]
                    x = (bounds["left"] + bounds["right"]) // 2
                    y = (bounds["top"] + bounds["bottom"]) // 2
                    response = device.shell(["input", "tap", str(x), str(y)], timeout=5)
                    if response.exit_code != 0:
                        raise RuntimeError("ADB 点击执行失败")
                elif action.operation == "set_text":
                    obj.set_text(action.text, timeout=2)
                    if obj.get_text(timeout=2) != action.text:
                        raise RuntimeError("输入文字与读取结果不一致")
                    details["text_verified"] = True
                else:
                    keys = {"back": "KEYCODE_BACK", "home": "KEYCODE_HOME", "up": "KEYCODE_DPAD_UP", "down": "KEYCODE_DPAD_DOWN", "left": "KEYCODE_DPAD_LEFT", "right": "KEYCODE_DPAD_RIGHT", "enter": "KEYCODE_DPAD_CENTER", "recent": "KEYCODE_APP_SWITCH", "volume_up": "KEYCODE_VOLUME_UP", "volume_down": "KEYCODE_VOLUME_DOWN", "play_pause": "KEYCODE_MEDIA_PLAY_PAUSE"}
                    response = device.shell(["input", "keyevent", keys[action.key]], timeout=5)
                    if response.exit_code != 0:
                        raise RuntimeError("ADB 按键执行失败")
                self.observation = None
                result = {"operation_id": str(action.operation_id), "state": "complete", **details}
                if action.observation_id is not None:
                    result["app"] = device.app_current()
                self.db.execute("UPDATE operations SET state='complete',result=? WHERE id=?", (compact(result), str(action.operation_id)))
                self.db.commit()
                completed = True
                return result
            finally:
                if not completed:
                    self.observation = None
                    self.db.execute("UPDATE operations SET state='interrupted' WHERE id=?", (str(action.operation_id),))
                    self.db.commit()


def create_app(state_dir: Path):
    token = (state_dir / "token").read_text().strip()
    if len(token) < 40:
        raise ValueError("服务令牌长度不足")
    controller = AndroidTools(state_dir)
    @asynccontextmanager
    async def lifespan(app):
        yield
        controller.disconnect()

    app = FastAPI(title="Android Tools", docs_url=None, redoc_url=None, lifespan=lifespan)

    @app.middleware("http")
    async def authorize(request: Request, call_next):
        if request.headers.get("origin"):
            return JSONResponse({"detail": "服务仅接受本地工具调用"}, status_code=403)
        supplied = request.headers.get("authorization", "")
        if not secrets.compare_digest(supplied.encode(), ("Bearer " + token).encode()):
            return JSONResponse({"detail": "服务鉴权失败"}, status_code=401)
        if request.method == "POST":
            length = request.headers.get("content-length", "")
            if not length.isdecimal() or request.headers.get("transfer-encoding"):
                return JSONResponse({"detail": "请求需要 Content-Length"}, status_code=411)
            if int(length) > MAX_REQUEST_BYTES:
                return JSONResponse({"detail": "请求超过长度上限"}, status_code=413)
        return await call_next(request)

    @app.get("/v1/health")
    def health():
        if controller.monitor_failed:
            raise HTTPException(503, "ADB 连接监听已经终止")
        return {"service": "android-tools", "protocol": 1, "uiautomator2": version("uiautomator2"), "adb_server": adbutils.adb.server_version()}

    @app.get("/v1/devices")
    def devices():
        return {"devices": controller.devices()}

    @app.post("/v1/connect")
    def connect(request: Connection):
        return controller.connect(request.serial)

    @app.get("/v1/observe")
    def observe():
        return controller.observe()

    @app.get("/v1/observations/{observation_id}")
    def page(observation_id: UUID, offset: int = 0):
        if offset < 0:
            raise HTTPException(422, "分页位置需要大于或等于零")
        return controller.page(observation_id, offset)

    @app.post("/v1/actions")
    def execute(request: Action):
        return controller.execute(request)

    @app.post("/v1/cancel")
    def cancel():
        return controller.cancel()

    @app.post("/v1/disconnect")
    def disconnect():
        return controller.disconnect()

    return app


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["init", "serve"])
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    state_dir = args.state_dir.resolve()
    if args.command == "init":
        state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
        os.chmod(state_dir, 0o700)
        with (state_dir / "token").open("x") as output:
            os.chmod(output.name, 0o600)
            output.write(secrets.token_urlsafe(32) + "\n")
        print("服务令牌已经写入指定目录")
    else:
        os.umask(0o077)
        uvicorn.run(create_app(state_dir), host="127.0.0.1", port=args.port, access_log=False)


if __name__ == "__main__":
    main()
