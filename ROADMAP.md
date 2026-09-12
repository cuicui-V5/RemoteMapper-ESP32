# 🗺️ RemoteMapper-ESP32 路线规划与功能规划 (Roadmap)

本文档汇总了社区用户的 Issue 需求反馈与项目后续的功能演进规划。

---

## 📌 近期规划与进行中功能 (In Progress / Planned)

### 1. 局域网网络唤醒 (Wake-on-LAN / WOL)
- **关联需求**：[Issue #1 (WOL功能建议)](https://github.com/cuicui-V5/RemoteMapper-ESP32/issues/1)
- **功能描述**：
  将按键（例如电源键长按或双击）绑定为发送局域网 WOL 魔术包（Magic Packet）。当电脑处于关机（S5）或睡眠（S3/S4）状态且支持网卡唤醒时，按下遥控器即可通过 ESP32-S3 发送 UDP 广播唤醒电脑，让遥控器成为真正的“电脑无线开机键”。
- **技术实现路径**：
  1. **Web 配置项**：在系统/网络配置页面中增加“目标电脑 MAC 地址”与“广播端口（默认 9）”设置，并持久化至 NVS。
  2. **按键动作扩展**：在按键映射引擎中新增 `ACTION_WOL` 动作类型，支持任意按键的单击/长按/双击绑定。
  3. **WOL 发送器**：基于 WiFi UDP 广播实现标准 Magic Packet 组包（`6 * 0xFF` + `16 * Target MAC`）。

---

### 2. 遥控器电池电量显示与低电量提醒
- **关联需求**：[Issue #5 (是否有办法在页面上补充电量信息和提示)](https://github.com/cuicui-V5/RemoteMapper-ESP32/issues/5)
- **功能描述**：
  在 Web 控制台的顶部状态栏以及“蓝牙配对管理”页面中，实时展示当前连接的遥控器电量百分比，并在电量过低（如低于 15%）时提供视觉提醒，方便用户及时更换电池。
- **技术实现路径**：
  1. **BLE 电池服务解析**：在 `ble_remote_client` 中探测标准蓝牙电池服务（Battery Service UUID: `0x180F`，Characteristic: `0x2A19`）。
  2. **GATT 订阅/读取**：连接成功后主动读取当前电量，并启用 Notification 监听电量变动。
  3. **API 与 Web 界面渲染**：在 `/api/status` 与 `/api/ble/info` 返回 JSON 中增加 `battery_pct` 字段；Web 界面增加动态电量图标（如 🔋 85%）。

---

## 🚀 中远期规划探索 (Future Exploration)

- [ ] **宏按键序列 (Key Macro Sequences)**：支持单个按键触发一连串按键或文本输入（带延时可调）。
- [ ] **多款蓝牙遥控器协议自动适配**：除小米标准蓝牙语音遥控器外，扩展支持常见的小米电视盒子其他型号及第三方 BLE 遥控器。
- [ ] **Web 蓝牙直连/本地辅助工具**：在免 Wi-Fi 模式下，支持基于 WebSerial / WebHID 直接在浏览器端配置按键。

---

## 💡 欢迎贡献
如果你有兴趣实现上述功能，或有其他功能建议，欢迎提交 PR 或在 [GitHub Issues](https://github.com/cuicui-V5/RemoteMapper-ESP32/issues) 中参与讨论！
