# 🗺️ RemoteMapper-ESP32 路线规划与功能规划 (Roadmap)

本文档汇总了社区用户的需求反馈、已落地实现的功能以及项目后续的演进规划。

---

## ✅ 已完成落地功能 (Completed)

### 1. 局域网网络唤醒 (Wake-on-LAN / WOL)
- **关联需求**：[Issue #1 (添加WOL功能的建议)](https://github.com/cuicui-V5/RemoteMapper-ESP32/issues/1)
- **实现版本**：master / v1.2+
- **功能特性**：
  - 在按键映射引擎中新增 `ACTION_WOL (11)` 动作模式。
  - 支持将任意按键的单击、长按或双击配置为唤醒目标电脑。
  - 自动向局域网广播标准 WOL Magic Packet（`6 * 0xFF` + `16 * 目标MAC`）。
  - 支持灵活的 MAC 输入格式（`AA:BB:CC:DD:EE:FF`、`AA-BB-CC-DD-EE-FF`、连续 12 位十六进制）。
  - WebUI 内置“发送测试唤醒包”快捷调试按钮及持久化存储。

### 2. 遥控器电池电量显示与低电量提醒
- **关联需求**：[Issue #5 (是否有办法在配置页面补全充电提醒和电量提示)](https://github.com/cuicui-V5/RemoteMapper-ESP32/issues/5)
- **实现版本**：master / v1.2+
- **功能特性**：
  - 自动发现并解析 BLE 标准电池服务（UUID `0x180F`，特征 `0x2A19`）。
  - 连接建立时主动采集电量，并支持 Notification 订阅与 30 分钟低频保底轮询。
  - WebUI 顶部状态栏与蓝牙详情面板实时展示精确电量（`🔋 85%`），离线与低电量自适应状态指示。
  - **板载硬件级提醒**：当遥控器电量 `<= 15%` 时，板载 RGB LED 会在正常连接状态下以 2 秒为周期执行白灯双脉冲闪烁（Double-pulse White Blink），提醒用户及时更换电池。

---

## 📌 近期与中远期规划 (Planned / Future)

### 1. 高级扩展模块落地
Web 控制台映射弹窗已完成“高级扩展”架构设计与交互占位符：
- [ ] **按键宏序列 (Key Macro Sequences)**：支持单个按键触发一连串按键或文本输入（带毫秒级延时可调）。
- [ ] **HTTP Webhook 网络请求**：支持按键触发向局域网/互联网 REST API 发送 GET/POST 请求，深度联动智能家居、家庭服务器（如 Home Assistant、Node-RED）。
- [ ] **MQTT 物联网推送**：支持按键触发向指定 MQTT Broker 发送自定义 Topic 与 Payload。

### 2. 硬件与生态兼容性扩展
- [ ] **更多型号蓝牙遥控器适配**：除小米蓝牙语音遥控器 2 Pro (RC003) 外，逐步适配常见的小米电视盒子系列及第三方标准 BLE 遥控器。
- [ ] **Web 蓝牙直连/本地辅助工具**：在免 Wi-Fi 模式下，探索基于 WebSerial / WebHID 直接在浏览器端免网配按键。

---

## 💡 欢迎贡献
如果你有兴趣实现上述规划，或有新的功能建议，欢迎提交 PR 或在 [GitHub Issues](https://github.com/cuicui-V5/RemoteMapper-ESP32/issues) 中发起讨论！
