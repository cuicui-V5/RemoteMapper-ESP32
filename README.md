<div align="center">

# ⚡ RemoteMapper-ESP32

**Windows 下小米蓝牙语音遥控器 2 Pro 的最佳免驱动硬件桥接方案**

基于 ESP32-S3 原生 USB OTG · 免驱动物理 UAC 1.0 麦克风 · 100% 捕获 13 个按键 · 5 层宏映射 · 局域网 WOL 远程开机

[![GitHub Release](https://img.shields.io/github/v/release/cuicui-V5/RemoteMapper-ESP32?color=06b6d4&label=Release&logo=github&style=flat-square)](https://github.com/cuicui-V5/RemoteMapper-ESP32/releases)
[![Official Website](https://img.shields.io/badge/Website-Online-10b981?logo=googlechrome&logoColor=white&style=flat-square)](https://cuicui-v5.github.io/RemoteMapper-ESP32/)
[![Web Flasher](https://img.shields.io/badge/Web%20Flasher-Ready-3b82f6?logo=espressif&logoColor=white&style=flat-square)](https://cuicui-v5.github.io/RemoteMapper-ESP32/flash.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-f59e0b?style=flat-square)](./LICENSE)
[![GitHub Stars](https://img.shields.io/github/stars/cuicui-V5/RemoteMapper-ESP32?color=8b5cf6&logo=github&style=flat-square)](https://github.com/cuicui-V5/RemoteMapper-ESP32/stargazers)

<p>
  <a href="https://cuicui-v5.github.io/RemoteMapper-ESP32/"><strong>🌐 官方网站</strong></a> •
  <a href="https://cuicui-v5.github.io/RemoteMapper-ESP32/flash.html"><strong>⚡ 网页在线刷机</strong></a> •
  <a href="https://github.com/cuicui-V5/RemoteMapper-ESP32/releases"><strong>📦 离线固件下载</strong></a> •
  <a href="./DEVELOPMENT.md"><strong>📖 完整技术全景文档</strong></a>
</p>

</div>

---

## 💡 为什么需要 RemoteMapper-ESP32？

在 Windows 下直接通过蓝牙连接小米蓝牙遥控器 2 Pro 时，存在两大致命系统级限制：

1. **部分物理按键被系统内核无情过滤**：
   遥控器的「音量+ (0x80)」、「音量- (0x81)」、「返回 (0xF1)」采用非标键码，Windows 自带蓝牙驱动（`kbdhid.sys`）会在内核层直接丢弃，应用层 Hook / AutoHotkey 完全截获不到。若编写内核驱动则必须开启测试模式，极易触发竞技游戏反作弊封禁。
2. **语音功能严重依赖第三方虚拟声卡**：
   私有 ATVV 压缩音频必须借助虚拟声卡（如 VB-Cable）二次路由，引入明显音频延迟且经常出现声道占用或采样率不匹配。

### 方案对比

| 对比维度 | 传统纯软件直连方案 | RemoteMapper-ESP32 硬件桥接方案 |
| :--- | :--- | :--- |
| **按键捕获完整度** | ❌ 音量键、返回键被内核过滤丢弃 | ✅ **13 个物理按键 100% 空口完整捕获** |
| **语音输入体验** | ❌ 依赖虚拟声卡二次路由，配置繁琐高延迟 | ✅ **免驱动硬件级 USB 物理麦克风 (UAC 1.0)**，插上即录 |
| **反作弊与安全性** | ❌ 过滤驱动需开启系统测试模式，易遭封禁 | ✅ **模拟标准免驱 HID 键盘**，游戏反作弊 100% 免疫 |
| **远程开机 / 唤醒** | ❌ PC 关机或深度睡眠时蓝牙断开，无法唤醒 | ✅ **支持局域网 WOL 远程网络开机**，沙发上一键开机 |
| **后台软件依赖** | ❌ 电脑必须常驻 Hook 软件与驱动进程 | ✅ **零后台运行**，芯片独立运算，即插即用 |

---

## ⚡ 极速开始：网页端免驱动一键刷机

> [!TIP]
> **官方首选推荐**：无需下载 Python、Git、PlatformIO 或任何串口驱动，准备一根 Type-C 数据线，在电脑浏览器中即可完成线刷！

👉 **[点击打开网页端在线刷机平台 (Web Serial Flasher)](https://cuicui-v5.github.io/RemoteMapper-ESP32/flash.html)**

### 刷机 3 步操作：

1. **硬件连接**：使用具备数据传输功能的 Type-C 数据线，连接 ESP32-S3 开发板标有 **`USB`** 的原生 OTG 接口（双口板切勿插在 COM/UART 口）。
2. **手动进入刷机模式（关键）**：
   > **按住开发板上的 `BOOT` 键不放 &rarr; 点按一下 `RST` 重启键松开 &rarr; 最后松开 `BOOT` 键**
   > *(大多数开发板无法自动拉低电平，必须按此物理组合键进入 Bootloader 状态)*
3. **一键烧录与重启**：
   * 打开 [网页刷机平台](https://cuicui-v5.github.io/RemoteMapper-ESP32/flash.html)，点击 **「连接开发板 (Connect)」**；
   * 系统将**自动读取 Flash 与 PSRAM 规格，并智能检测是否已安装过本项目**（新板自动全量刷写，老用户自动推荐保留配置升级）；
   * 点击 **「立即开始刷机」**，约 15 秒写入完成；
   * **👉 烧录完成后，必须按一下开发板上的物理【RST】按键重启设备！**

---

## ✨ 核心特性

* 🎙️ **免驱动 UAC 1.0 硬件级物理麦克风**  
  模拟标准 16kHz 16-Bit 单声道输入设备。芯片内置 4-bit IMA-ADPCM 实时流解码、Declip 尖峰消除、3-Tap FIR 滤波与动态 AGC 增益控制。按住语音键瞬间自动注入输入法快捷键，并施加 150ms 杂音消隐与 10ms 淡入平滑，彻底消除微动按键爆音。
* 🎛️ **5 层独立宏映射系统**  
  提供默认层与 4 个独立自定义层。每个按键均支持**单击、长按（200~2000ms 可调）与双击（100~600ms 可调）**。具备穿透继承、一次性层、闲置超时自动返回与 RGB 状态指示灯。
* 📡 **局域网远程无线开机 (Wake-on-LAN / WOL)**  
  可将任意按键（甚至长按动作）绑定为局域网网络唤醒魔术包（Magic Packet）。无论电脑处于关机（S5）还是睡眠状态，轻按遥控器即可唤醒开机。
* 🌐 **内置嵌入式可视化 Web 控制台**  
  ESP32 固件集成 Web 服务器与 AP 热点（`RemoteMapper-AP`）。手机或电脑连接后在网页中即可直观修改按键映射，**设备连入家庭路由器后自动关闭 AP 射频以降低发热与功耗**。配置存储于 Flash NVS 掉电不丢。
* 🔋 **遥控器电量实时监控与低电量预警**  
  自动监听 BLE 标准电池服务，Web 后台实时展示电量百分比；当遥控器电量 $\le 15\%$ 时，板载 RGB 指示灯以双闪白光警示更换电池。
* 🧩 **主流 ESP32-S3 硬件全系支持**  
  开箱适配主流 ESP32-S3-WROOM-1 开发板，包括 N16R8（16MB Flash/8MB PSRAM）、N8R8、N8R2、N4R2 等，刷机网页自动检测匹配。

---

## 🎮 默认出厂按键映射

| 遥控器物理按键 | 物理键码 | 单击行为 | 长按行为 | 双击行为 |
| :--- | :--- | :--- | :--- | :--- |
| **电源键** | `0x66` | `Alt + F4` (关闭当前窗口) | 系统休眠 (`0x0032`) | — |
| **语音键** | `0x04` | 麦克风音频推流 + `RAlt + ,` (微信语音输入) | 按住持续录音 | — |
| **方向上 / 下** | `0x52 / 0x51` | 键盘方向键 `↑` / `↓` | — | — |
| **方向左 / 右** | `0x50 / 0x4F` | 键盘方向键 `←` / `→` (播放快进/快退) | — | — |
| **确定键 (OK)** | `0x28` | 回车键 (`Enter`) | — | — |
| **返回键** | `0xF1` | 多媒体返回 (`AC Back / 0x0224`) | — | — |
| **主页键** | `0x24` | 显示桌面 (`Win + D`) | — | — |
| **菜单键** | `0x5D` | 播放 / 暂停 (`Space`) | — | — |
| **音量+ / 音量-** | `0x80 / 0x81` | 系统全局音量增加 (`0x00E9`) / 减小 (`0x00EA`) | 持续平滑调节 | — |
| **电视键 (TV)** | `0xC0` | 快捷键 `F8` (可用于触发伴侣脚本) | — | — |

> [!NOTE]
> 以上所有按键的行为、长按时间、双击间隔均可在内置 Web 控制台中自由修改、互换或清除。

---

## 📖 5 步快速上手指南

```text
[1. 手动按 RST 重启] ──► [2. 手机连接 RemoteMapper-AP] ──► [3. 打开 http://192.168.4.1]
                                                                    │
[5. 开始使用享受桌面] ◄── [4. 按住遥控器「主页+菜单」配对] ◄────────┘
```

1. **重启设备**：固件烧录完成后，按一下开发板上的 **【RST】** 实体按键启动系统（板载 RGB 彩灯亮起即进入就绪状态）。
2. **连接配置热点**：使用手机或电脑连接名为 **`RemoteMapper-AP`** 的 Wi-Fi 热点（默认无密码）。
3. **打开 Web 后台**：浏览器访问 **`http://192.168.4.1/`**（或局域网域名 **`http://remotemapper.local/`**）。可在后台「网络设置」中将设备连入家庭 Wi-Fi。
4. **一键配对遥控器**：
   * 拿起小米遥控器 2 Pro，**同时按住「主页键」与「菜单键」持续约 3~5 秒**；
     > *(★ 注意：该款遥控器进入配对广播时表面**没有任何声光提示或指示灯反馈**，这是完全正常的物理特性，直接按住几秒即可)*
   * 在 Web 后台「蓝牙配对」页面点击「开始扫描」，找到设备点击连接绑定。
5. **配置语音输入联动**：
   * 打开 Windows「设置」➔「系统」➔「声音」，在**输入设备列表**中确认已新识别到 **`RemoteMapper Mic`** 硬件麦克风（**无需**将其设为系统全局默认设备，不影响日常耳机与游戏开黑）；
   * 打开常用语音输入法（如**微信输入法**、**讯飞输入法**等）设置，将**语音输入麦克风单独指定为 `RemoteMapper Mic`**；
   * 将输入法内的语音快捷键设置为与 Web 控制台中一致（出厂默认预设为 **`右Alt + 逗号`**）。现在按住遥控器语音键说话，即可享受超低延迟的无线语音转文字！

---

## 🛠️ 进阶：源码自主编译与开发

如果您需要深度二次开发（如扩展协议栈、修改 BLE 广播过滤、重定向引脚），可使用 PlatformIO 进行编译：

```bash
# 克隆本仓库
git clone https://github.com/cuicui-V5/RemoteMapper-ESP32.git
cd RemoteMapper-ESP32

# 编译所有板型 (N16R8 / N8R8 / N8R2 / N4R2)
build.bat all

# 直接指定型号线刷
flash.bat n16r8

# 运行串口实时监控
monitor.bat
```

详细的系统多核调度、DSP 音频流水线、NVS 自愈存储机制与 RESTful API 规范，请参阅：  
👉 **[完整技术全景与架构开发手册 (DEVELOPMENT.md)](./DEVELOPMENT.md)**

---

## 💖 赞助与支持

**RemoteMapper-ESP32 作为社区最早将小米蓝牙语音遥控器完整适配到 ESP32 硬件桥接的开创性开源项目之一**，率先攻克了 Windows 下特定按键被系统底层驱动丢弃的历史痛点，开创性地在 ESP32-S3 双核架构下完整实现了 **BLE 遥控捕获 + USB HID 复合设备模拟 + UAC 实时无线麦克风音频推流 + 网页端可视化配置与免环境刷写工具**。

作者投入了大量的业余心血与精力进行底层逆向与打磨，并将整套成熟稳定的软硬件方案**完全开源、永久免费分享给社区**。

如果你觉得本项目为你解决了遥控器痛点、带来了便利，欢迎自愿请作者喝一杯咖啡 ☕ / 冰阔落 🥤！你的支持是持续维护与迭代新特性的最大动力 ❤️。

<div align="center">
  <img src="./docs/images/wechat_reward.png" width="260" alt="微信赞赏码" />
  <p><em>微信赞赏码（感谢每一位支持开源的朋友 ❤️）</em></p>
</div>

---

## ⭐ Star History

<div align="center">
  <a href="https://star-history.com/#cuicui-V5/RemoteMapper-ESP32&Date">
    <img src="https://api.star-history.com/svg?repos=cuicui-V5/RemoteMapper-ESP32&type=Date" alt="Star History Chart" />
  </a>
</div>

---

## 📄 鸣谢与开源许可证

* [QL-4/RemoteMapper](https://github.com/QL-4/RemoteMapper)：提供了 Windows 纯软件方案与 ATVV 协议分析参考。
* [godarrenw/mi_remote_control](https://github.com/godarrenw/mi_remote_control)：提供了遥控器蓝牙协议分析参考。
* [Espressif esptool-js](https://github.com/espressif/esptool-js)：为网页端 Web Serial 在线刷机平台提供了底层通信支持。

本项目遵循 [MIT License](./LICENSE) 协议开源。
