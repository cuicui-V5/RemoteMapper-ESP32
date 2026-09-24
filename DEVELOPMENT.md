# RemoteMapper-ESP32 开发者手册与技术全景文档
## 小米蓝牙语音遥控器 2 Pro (RC003) ➔ USB 复合硬件桥接固件

> **适用固件版本**：v1.0.0+ (Production Release)  
> **目标主控硬件**：ESP32-S3-DevKitC-1 N16R8（16MB Octal SPI Flash + 8MB Octal SPI PSRAM）  
> **适配遥控型号**：小米蓝牙语音遥控器 2 Pro（型号：RC003，铝合金金属机身款）  
> **文档受众**：嵌入式开发者、代码维护者与后续 AI Agent 协作开发  

---

# 目录
1. [系统整体架构与多核任务分配](#一-系统整体架构与多核任务分配)
2. [硬件选型与存储分区设计 (16MB Flash)](#二-硬件选型与存储分区设计-16mb-flash)
3. [BLE 与 ATVV 语音协议栈全流程解析](#三-ble-与-atvv-语音协议栈全流程解析)
4. [DSP 实时音频处理流水线算法设计](#四-dsp-实时音频处理流水线算法设计)
5. [多触发按键映射状态机与键码引擎](#五-多触发按键映射状态机与键码引擎)
6. [Flash NVS 存储引擎与开机自愈容灾机制](#六-flash-nvs-存储引擎与开机自愈容灾机制)
7. [USB 复合设备架构 (UAC 1.0 + HID + CDC)](#七-usb-复合设备架构-uac-10--hid--cdc)
8. [Wi-Fi 双模网络与嵌入式 Web 控制台](#八-wi-fi-双模网络与嵌入式-web-控制台)
9. [PC 本地伴侣程序架构 (pc_companion)](#九-pc-本地伴侣程序架构-pc_companion)
10. [RESTful API 完整接口规范](#十-restful-api-完整接口规范)
11. [源码文件索引与模块依赖关系](#十一-源码文件索引与模块依赖关系)
12. [编译、烧录与测试指南](#十二-编译烧录与测试指南)
13. [故障排查手册与常见问题 (Troubleshooting)](#十三-故障排查手册与常见问题-troubleshooting)

---

# 一. 系统整体架构与多核任务分配

ESP32-S3 配备 Xtensa® 双核 32 位 LX7 微处理器（主频 240MHz）。为了保证实时音频解码 0 掉帧、USB Isochronous 报文 1000Hz 零抖动、BLE 协议栈秒级响应以及 Web 异步请求不卡顿，系统采用 **FreeRTOS 双核物理隔离调度架构**：

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                ESP32-S3 核心分配与流水线                                  │
├───────────────────────────────────────────┬────────────────────────────────────────────┤
│           Core 0 (专职蓝牙与 DSP)          │         Core 1 (专职 USB、网络与业务)        │
├───────────────────────────────────────────┼────────────────────────────────────────────┤
│ • NimBLE Central 协议栈 (扫描/连接/绑定)   │ • TinyUSB 复合设备 (UAC Mic + HID + CDC)   │
│ • HOGP 原始报文解析 (订阅 0x2A4D)         │ • UAC 麦克风 1ms Isochronous 数据泵 (16kHz)│
│ • ATVV 语音握手 (0x0A/0x0C/0x0E/0x00)     │ • Multi-Trigger 状态机 (单击/长按/双击)    │
│ • 4-bit IMA-ADPCM 实时流式解码             │ • Flash NVS 存储引擎 & 自愈容灾校验        │
│ • Declip 尖峰消除 + 3-Tap FIR 低通滤波    │ • Wi-Fi 双模管理器 (AP 192.168.4.1 + STA)  │
│ • 动态 AGC 增益控制与软削波                │ • Captive Portal DNS 强制门户 (Port 53)    │
│ • 音频数据注入 SPSC 无锁环形缓冲区         │ • HTTP WebServer (Port 80 RESTful API)     │
│ • RGB 状态指示灯控制器 (WS2812 / GPIO48)  │ • Serial / CDC CLI 控制台指令调度          │
└───────────────────────────────────────────┴────────────────────────────────────────────┘
                                     │                     ▲
                                     └────── RingBuffer ───┘
                                         (4096 Samples PCM)
```

---

# 二. 硬件选型与存储分区设计 (16MB Flash)

### 1. 硬件连接规范
* **主控型号**：ESP32-S3-DevKitC-1 N16R8（16MB Flash + 8MB Octal PSRAM）；
* **USB 接口**：必须连接 ESP32-S3 开发板的 **`USB/OTG` 接口**（对应内部硬件 GPIO 19: D-，GPIO 20: D+），**不可连接 UART 口**；
* **遥控器型号**：**小米蓝牙语音遥控器 2 Pro（型号：RC003）**。
* **供电**：USB 5V 直接供电，典型运行功耗约 110~130mA。

### 2. 16MB Flash 分区表 (`default_16MB.csv`)

| 分区名 | 类型 | 子类型 | 起始偏移 (Offset) | 大小 (Size) | 用途说明 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `nvs` | data | nvs | `0x9000` | 24 KB (`0x6000`) | BLE 配对密钥、Wi-Fi 配置、自定义按键存储 |
| `otadata` | data | ota | `0xf000` | 8 KB (`0x2000`) | OTA 升级状态标记 |
| `app0` | app | ota_0 | `0x20000` | 6 MB (`0x600000`) | 当前运行的主固件镜像 |
| `app1` | app | ota_1 | `0x620000` | 6 MB (`0x600000`) | 后续 OTA 固件备用镜像 |
| `spiffs` | data | spiffs | `0xc20000` | 3.75 MB (`0x3c0000`)| 嵌入式静态资源 / 扩展文件系统 |
| `coredump` | data | coredump | `0xfe0000` | 128 KB (`0x20000`) | 系统崩溃转储诊断日志 |

---

# 三. BLE 与 ATVV 语音协议栈全流程解析

### 1. GATT 服务与特征值定义
* **HOGP 规范服务**：UUID `0x1812` ➔ HID Report 特征值 `0x2A4D`（Notify，捕获按键原始报文）；
* **ATVV 自定义服务**：UUID `ab5e0001-5a21-4f05-bc7d-af01f617b664`
  * `ab5e0002-5a21-4f05-bc7d-af01f617b664` (**CMD 通道**，Write Without Response，下发握手指令)
  * `ab5e0003-5a21-4f05-bc7d-af01f617b664` (**AUDIO 通道**，Notify，接收 120 字节 ADPCM 音频帧)
  * `ab5e0004-5a21-4f05-bc7d-af01f617b664` (**CTL 通道**，Notify，接收语音启停状态)

### 2. ATVV 握手与语音状态机时序
```text
ESP32-S3 (Central)                                      小米遥控器 RC003 (Peripheral)
      │                                                           │
      │── 1. 扫描匹配 "MI RC" 或 MAC 前缀 "c0:5d:39" ─────────────►│
      │◄─ 2. 建立 BLE GATT 连接并启用 Bonding ─────────────────────│
      │                                                           │
      │── 3. 订阅 CTL(0x04)、AUDIO(0x03)、HOGP(0x2A4D) 特征值 ───►│
      │                                                           │
      │── 4. 发送 GET_CAPS: [0x0A, 0x01, 0x00, 0x00, 0x03, 0x03] ─►│
      │◄─ 5. 收到 CAPS_RESP [0x0B, ...]: 确认 16kHz ADPCM & 帧大小 ──│
      │                                                           │
      │── 6. 发送 MIC_OPEN: [0x0C, 0x00] ─────────────────────────►│
      │      (遥控器进入 HTT 按住说话待命模式)                     │
      │                                                           │
      │  ═════════════════ 用户按住遥控器语音键 ═════════════════   │
      │◄─ 7. CTL 通道上报 AUDIO_START HTT: [0x04, 0x03, 0x00, SID]│
      │      • 固件重置 ADPCM 预测器 & 滤波器                     │
      │      • 触发 USB HID 注入语音热键 (RAlt + ,)                │
      │      • 开启 UAC 麦克风音频推流                            │
      │                                                           │
      │◄─ 8. AUDIO 通道持续推送 ADPCM 数据帧 (每帧 120 字节) ──────│
      │      • 解码为 240 个 16-bit PCM 采样点并写入 RingBuffer    │
      │                                                           │
      │── 9. 每隔 2000ms 发送 MIC_EXTEND: [0x0E, SID] ───────────►│
      │      (延长语音会话防遥控器超时自动断流)                   │
      │                                                           │
      │  ═════════════════ 用户松开遥控器语音键 ═════════════════   │
      │◄─ 10. CTL 通道上报 AUDIO_STOP: [0x00, 0x02] ──────────────│
      │       • 释放语音热键                                      │
      │       • 停止麦克风推流，清空环形缓冲区                    │
      │       • 重新发送 MIC_OPEN [0x0C, 0x00] 防御下次按键失效    │
```

---

# 四. DSP 实时音频处理流水线算法设计

### 1. 4-bit IMA-ADPCM 流式解码
- **采样规格**：16000 Hz 单声道 16-bit PCM；
- **帧解析**：BLE 每帧 120 字节（高 4 位优先，低 4 位其次），对应解出 240 个 PCM 采样点；
- **跨帧连续性**：Predictor 与 StepIndex 在帧间保持连续，收到 `AUDIO_SYNC`（`0x0A`）时重置同步点。

### 2. Declip 单点尖峰消除
消除蓝牙丢包或电磁干扰引起的孤立单点脉冲毛刺：
$$\text{若 } |x_i - x_{i-1}| > 1000 \text{ 且 } |x_i - x_{i+1}| > 1000 \text{ 且 } \min(dp, dn) > 2 \cdot |x_{i+1} - x_{i-1}| \implies x_i = \frac{x_{i-1} + x_{i+1}}{2}$$

### 3. 3-Tap 三角 FIR 低通平滑滤波器
抑制高频量化白噪，提升人声音质厚度与转写清晰度：
$$y_i = \frac{x_{i-1} + 2x_i + x_{i+1}}{4}$$

### 4. 动态 AGC (自动增益控制)
- **目标电平**：`28000.0`
- **包络衰减率**：`0.9997`（约 144ms 慢释放，避免呼吸底噪）；
- **最大增益限制**：`30.0` 倍（避免空闲无声时放大本底噪声）；
- **软削波**：防止波形硬切顶产生爆音。

---

# 五. 多触发按键映射状态机与键码引擎

### 1. 遥控器全按键物理编码转换表

| 遥控器物理按键 | 小米原始 HID 报文 | ESP32 输出的 USB 信号 | 功能效果 |
| :--- | :--- | :--- | :--- |
| **音量+** | `0x80` | `Consumer: Volume Up (0x00E9)` | Windows 原生系统全局音量增加 |
| **音量-** | `0x81` | `Consumer: Volume Down (0x00EA)`| Windows 原生系统全局音量减小 |
| **返回键** | `0xF1` | `Consumer: AC Back (0x0224)` | 浏览器/文件管理器后退、播放器退出全屏 |
| **语音键** | ATVV 事件 | **注入 `RAlt + ,` + UAC 麦克风** | 唤醒微信输入法语音并实时录音 |
| **电源键** | `0xFF` / `0x66` | **单击 `Alt + F4` / 长按 `Sleep (0x0032)`** | 关闭当前窗口 / 系统睡眠 |
| **主页键** | `0x24` | `Keyboard: Win + D` | 一键返回桌面 |
| **菜单键** | `0x5D` | `Keyboard: Space` | 视频播放 / 暂停 |
| **电视键 (TV)**| `0xC0` | `Keyboard: F8` | 自定义快捷键 / 触发伴侣脚本 |
| **方向上/下/左/右** | `0x52/51/50/4F`| `Keyboard: Up/Down/Left/Right` | 播放器快进快退调进度 |
| **确定键 (OK)** | `0x28` | `Keyboard: Return` | 确认 / 回车 |

### 2. Multi-Trigger 三重触发状态机设计 (`key_state_machine.c`)
每个按键独立维护一个插槽状态结构体，支持 3 种互不干扰的手势：
* **单击（Click）**：瞬时点按或按下保持（Hold 直通）；
* **长按（Long Press）**：按压时长超过 `long_ms`（默认 600ms，支持 200~2000ms 自定义），触发长按动作，松开时不触发短按；
* **双击（Double Click）**：按键在 `double_ms`（默认 250ms，支持 100~600ms 自定义）内再次按下，触发双击动作；
* **防卡键瞬时脉冲机制**：超时触发的长按操作，释放时会补发释放包，彻底杜绝键值卡死。

### 3. 全链路 16 进制 / 10 进制双模解析引擎
- 键码统一支持 `0x...` 标准 16 进制与 10 进制无缝互通；
- `parse_u32_or_hex()` 智能识别数字或 `"0x2C"` 字符串格式。

---

# 六. Flash NVS 存储引擎与开机自愈容灾机制

为确保在 ESP32-S3 Flash 单扇区（4KB）限制下可靠持久化，存储系统采用分层加固设计：

```text
┌────────────────────────────────────────────────────────┐
│               Flash NVS 存储安全防御体系                 │
├────────────────────────────────────────────────────────┤
│ 1. 紧凑型 JSON 序列化 (未启用手势字段自动省略, < 1.4KB)   │
│ 2. 局部原子事务 (Preferences.begin -> putString -> end) │
│ 3. 强制刷写提交 (nvs_flash_commit)                     │
│ 4. 开机自愈容灾 (校验失败/坏块自动降级为 13 键出厂默认) │
│ 5. 底层分区防误删 (phy / nimble_bond / net80211 只读保护)│
└────────────────────────────────────────────────────────┘
```

1. **紧凑型 JSON 压缩**：仅序列化启用的触发手势，13 键全部配置 3 触发时总数据量仅约 1.2KB（< 35% 扇区容量）；
2. **开机自愈保护**：`key_config_storage_load()` 启动时严格校验 JSON 完整性，若异常断电损坏，自动恢复出厂默认值，杜绝系统变砖。

---

# 七. USB 复合设备架构 (UAC 1.0 + HID + CDC)

ESP32-S3 原生 USB OTG 在枚举时向操作系统呈现标准**复合多接口设备 (Composite Device)**：

1. **Interface 0 & 1：USB Audio Class 1.0 (物理麦克风)**
   - 格式：PCM 16-bit Mono, 16000Hz；
   - 端点：Isochronous IN 端点；
   - 独创 **bInterval=2 / wMaxPacketSize=64** 降频缓冲设计，完美绕过 DWC2 控制器无 SOF 中断引起的 500Hz 丢帧魔咒；
   - **硬件 SOF / DSTS 状态感知**：电脑休眠唤醒时自动重置 UAC 状态机。

2. **Interface 2：USB HID Keyboard**
   - 标准 6KRO 键盘报文，负责发送 `Win+D`, `Alt+Tab`, `RAlt+,`, `F13~F24` 等修饰组合键。

3. **Interface 3：USB HID Consumer Control**
   - 16-bit Usage 控制报文，负责发送原生系统音量（`0x00E9`/`0x00EA`）、静音（`0x00E2`）与 AC 后退（`0x0224`）。

4. **Interface 4：USB CDC Serial**
   - 虚拟串口，用于串口监控与 JSON 命令行交互。

---

# 八. Wi-Fi 双模网络与嵌入式 Web 控制台

### 1. 网络拓扑与低功耗调度
* **AP 模式（配置热点）**：
  - SSID: `RemoteMapper-AP` (开放免密或 WPA2 加密)
  - IP: `192.168.4.1` (子网掩码 `255.255.255.0`)
  - DNS Captive Portal: 自动拦截探测域名并重定向到 `192.168.4.1`。
  - **动态休眠降功耗**：一旦 STA 成功连上路由器并获取 IP，立刻自动关闭 SoftAP 射频发射以降低发热与功耗；若路由器断网超过 15 秒则自动重新开启 AP 防失联。
* **STA 模式（局域网接入）**：
  - 连接路由器 2.4GHz Wi-Fi；
  - mDNS: 局域网统一访问地址为 **`http://remotemapper.local`**。

### 2. Web 前端架构 (`src/web/web_ui.h`)
- **纯原生单页架构**：HTML/CSS/JS 经 PROGMEM 嵌入 Flash，零外部 CDN 依赖；
- **1:1 遥控器仿真面板**：实体按键按下时界面实时高光联动；
- **敲击即录制**：改键窗口直接按下键盘即可自动识别并填入 HID 键码；
- **NVS 纯文本高级编辑器**：内置语法校验、格式化与一键备份导出/导入。

---

# 九. PC 本地伴侣程序架构 (`pc_companion`)

为了弥补 USB HID 键盘无法直接执行本地程序/脚本的不足，随固件配备了独立的 Windows 伴侣守护进程：

```text
┌────────────────────────┐      USB HID Keycode (F13 ~ F24)      ┌────────────────────────┐
│                        │ ────────────────────────────────────► │  Windows 宿主机 PC     │
│   ESP32-S3 硬件桥接器  │                                       │                        │
│   (发送零冲突高阶 F 键) │                                       │  pc_companion 守护进程 │
└────────────────────────┘                                       │  (Win32 RegisterHotKey)│
                                                                 └───────────┬────────────┘
                                                                             │ 异步派发
                                                                             ▼
                                                                 ┌────────────────────────┐
                                                                 │ • 打开网页 (抖音/Gemini)│
                                                                 │ • 批处理 (.bat 切 PS5) │
                                                                 │ • PowerShell / Python  │
                                                                 │ • 启动应用程序 (Steam) │
                                                                 └────────────────────────┘
```

* **零冲突暗号**：使用 Windows 系统预留但物理键盘没有的 `F13 ~ F24` 键码；
* **极速无感**：原生 Win32 `RegisterHotKey` 消息循环，待机 CPU 0.0%，内存仅几 MB；
* **热重载配置**：修改 `config.json` 自动生效无需重启。

---

# 十. RESTful API 完整接口规范

| 请求方法 | 路由 Path | 描述 | 请求体示例 | 响应体示例 |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/status` | 获取系统全量运行状态与统计 | 无 | `{"version":"1.0.0","ble_state":3,"frames_decoded":1240,"free_heap":241500,"sta_ip":"192.168.2.179"}` |
| `GET` | `/api/logs` | 获取最近 250 行环形运行日志 | 无 | `{"logs":["[0012.340] [BLE] Connected","[0014.120] [VOICE] PRESSED"]}` |
| `POST`| `/api/logs/clear` | 清空运行日志缓冲区 | 无 | `{"status":"cleared"}` |
| `GET` | `/api/wifi/scan` | 扫描周围 2.4GHz Wi-Fi | 无 | `{"networks":[{"ssid":"Home-WiFi","rssi":-58,"secure":true}]}` |
| `POST`| `/api/wifi/config` | 配置并连接家庭 Wi-Fi | `{"ssid":"MyHome","pass":"12345678"}` | `{"status":"ok"}` |
| `GET` | `/api/keymap` | 获取当前按键映射 JSON | 无 | `{"ver":2,"bindings":[{"source_vk":102,"has_click":true,...}]}` |
| `POST`| `/api/keymap/save` | 保存按键映射到 Flash NVS | `{"ver":2,"bindings":[...]}` | `{"status":"saved"}` |
| `POST`| `/api/keymap/reset` | 重置按键映射为出厂默认 | 无 | `{"status":"reset_ok"}` |
| `GET` | `/api/keymap/telemetry`| 获取当前按键实时触发遥测 | 无 | `{"source_vk":102,"is_pressed":true,"duration_ms":120,"key_code":44}` |
| `GET` | `/api/ble/scan` | 扫描周围可连接的蓝牙设备 | 无 | `{"devices":[{"name":"Xiaomi Voice Remote","mac":"c0:5d:39:...","rssi":-45}]}` |
| `POST`| `/api/ble/connect` | 连接并绑定指定 MAC 遥控器 | `{"mac":"c0:5d:39:c3:94:ab"}` | `{"status":"connected"}` |
| `POST`| `/api/ble/unpair` | 解绑当前遥控器并清空 NVS | 无 | `{"status":"unpaired"}` |
| `GET` | `/api/ble/info` | 获取当前绑定的遥控器信息 | 无 | `{"name":"Xiaomi Voice Remote","mac":"c0:5d:39:...","bound":true}` |
| `POST`| `/api/ble/reconnect` | 手动触发重新扫描与连接 | 无 | `{"status":"reconnecting"}` |
| `GET` | `/api/nvs` | 获取全量 NVS 命名空间纯文本 | 无 | `{"wifi_conf":{"ssid":"..."},"keymap_conf":{"cfg_json":{...}}}` |
| `POST`| `/api/nvs/save` | 将编辑后的 JSON 写入 NVS | `{"keymap_conf":{"cfg_json":{...}}}` | `{"status":"saved","message":"NVS写入成功"}` |
| `POST`| `/api/nvs/reset` | 清空 NVS 扇区恢复出厂重启 | 无 | `{"status":"erased","message":"NVS已清空..."}` |
| `POST`| `/api/system/restart` | 软重启 ESP32-S3 | 无 | `{"status":"rebooting"}` |

---

# 十一. 源码文件索引与模块依赖关系

```text
D:\tool\RemoteMapper-ESP32/
├── platformio.ini              # PlatformIO 构建配置文件 (Flash/PSRAM/编译宏)
├── default_16MB.csv            # 16MB Flash 分区表
├── build.bat                   # 一键编译脚本
├── flash.bat                   # 一键烧录脚本
├── monitor.bat                 # 串口监视器脚本
├── test.bat                    # 单元测试运行脚本
├── include/
│   ├── app_config.h            # 全局配置 (引脚、UUID、缓冲区与看门狗宏)
│   └── version.h               # 固件版本号
├── src/
│   ├── main.cpp                # 系统主入口与 FreeRTOS 双核任务调度
│   ├── led_indicator.h/.cpp    # WS2812 RGB 状态指示灯驱动 (蓝/绿/黄/红呼吸灯)
│   ├── log/
│   │   ├── app_log.h           # 全局格式化日志接口
│   │   └── app_log.cpp         # 250 行环形内存日志缓冲区实现
│   ├── audio/                  # DSP 实时音频处理流水线
│   │   ├── adpcm_decoder.h/.c  # 4-bit IMA-ADPCM 流式解码器
│   │   ├── audio_filter.h/.c   # Declip 尖峰消除与 3-Tap FIR 低通平滑
│   │   ├── audio_agc.h/.c      # 动态 AGC 增益控制与软削波
│   │   ├── audio_ring_buffer.h/.c # SPSC 无锁环形缓冲区
│   │   └── audio_pipeline.h/.c # 音频流水线主协调器
│   ├── ble/                    # 蓝牙与协议栈模块
│   │   ├── ble_remote_client.h # BLE Central 状态机接口
│   │   └── ble_remote_client.cpp # NimBLE 驱动、HOGP 监听与 ATVV 握手客户端
│   ├── usb/                    # USB 复合设备模块
│   │   ├── usb_composite.h     # TinyUSB 抽象接口
│   │   └── usb_composite.cpp   # UAC 1.0 麦克风 + HID 键盘 + Consumer 控制器
│   ├── keymap/                 # 按键映射与手势引擎
│   │   ├── key_definitions.h   # 遥控器物理键码与 USB HID 键码定义表
│   │   ├── key_state_machine.h # Multi-Trigger 状态机接口
│   │   ├── key_state_machine.c # 单击/长按/双击手势引擎实现
│   │   ├── key_config_storage.h# 按键映射 Flash NVS 持久化接口
│   │   └── key_config_storage.cpp# 紧凑 JSON 序列化、自愈容灾与十六进制解析
│   ├── nvs/                    # 底层 Flash 分区管理
│   │   ├── nvs_manager.h       # NVS 遍历、读写与重置接口
│   │   └── nvs_manager.cpp     # nvs_entry_find 动态扫描与安全防护
│   ├── wifi/                   # 网络模块
│   │   ├── wifi_manager.h      # Wi-Fi 管理接口
│   │   └── wifi_manager.cpp    # AP 192.168.4.1、DNS 强制门户与 mDNS
│   ├── web/                    # 嵌入式 Web 控制台
│   │   ├── web_ui.h            # HTML/CSS/JS 单页应用 (PROGMEM 存储)
│   │   ├── web_server.h        # WebServer 接口
│   │   └── web_server.cpp      # 18+ 条 RESTful API 路由处理
│   └── cli/                    # 串口诊断模块
│       ├── cli_manager.h       # CDC CLI 接口
│       └── cli_manager.cpp     # JSON 命令行解析器
├── pc_companion/               # PC 本地伴侣程序
│   ├── remotemapper_host.py    # Win32 RegisterHotKey 高性能守护进程
│   ├── config.json             # F13 ~ F24 本地脚本绑定配置文件
│   ├── start_companion.bat     # 前台调试启动脚本
│   ├── silent_start.vbs        # 后台静默启动脚本
│   ├── enable_autostart.bat    # 注册 Windows 开机自启
│   └── build_exe.bat           # PyInstaller 一键打包独立 EXE
└── test/
    └── native/test_suite.py    # 原生算法单元验证测试套件
```

---

# 十二. 编译、烧录与测试指南

### 1. 编译固件
```cmd
build.bat
```
*(底层调用 `python -m platformio run -e esp32s3_n16r8`)*

### 2. 烧录固件
将 ESP32-S3 开发板通过 USB 数据线连接到电脑的 **`USB/OTG`** 接口：
```cmd
flash.bat
```

### 3. 查看实时串口调试日志
```cmd
monitor.bat
```

### 4. 运行单元测试
```cmd
test.bat
```

---

# 十三. 故障排查手册与常见问题 (Troubleshooting)

### Q1: 遥控器按键没有反应，串口一直显示 `Scanning for Xiaomi Remote...`？
- **排查步骤**：
  1. 遥控器是否已被电脑原本的蓝牙占用？请在 Windows 蓝牙设置中先点击「删除设备/断开连接」；
  2. 同时按住遥控器的 **主页键 + 菜单键** 进行配对，直到遥控器指示灯快闪；
  3. 网页后台进入「📡 蓝牙配对管理」，点击「🔍 扫描附近蓝牙设备」找到 `Xiaomi Voice Remote` 点击连接。

### Q2: 语音键按下后输入法唤醒了，但没有声音录入？
- **排查步骤**：
  1. 打开 Windows「声音设置」➔「录制设备」，查看是否存在 `RemoteMapper Mic`；
  2. 将微信输入法或讯飞输入法的默认麦克风选择为 **`RemoteMapper Mic`**；
  3. 确认输入法快捷键与 Web 控制台中语音键绑定的热键一致（默认为 `右Alt + 逗号`）。

### Q3: 手机连接 `RemoteMapper-AP` 后无法打开配置界面？
- **排查步骤**：
  1. 手机连上热点后若提示「当前 Wi-Fi 无法访问互联网」，请选择「保持连接」；
  2. 手动在手机浏览器地址栏输入 **`http://192.168.4.1`**；
  3. 确保电脑/手机未开启全局代理或 VPN。

### Q4: 音量加减在 Windows 下变成打开搜索栏？
- **排查步骤**：
  1. 检查 WebUI 下拉菜单或 NVS 中音量键的键码，音量加标准 USB Consumer 键码为 **`0x00E9` (233)**，音量减为 **`0x00EA` (234)**；切勿使用旧版错误的 `545`（`0x0221` 代表 AC Search 全局搜索）。

---

## 📄 开源许可证
本项目遵循 [MIT License](./LICENSE) 开源许可协议。

