# Mac 遥控鼠标与 Android Agent

ESP32 将遥控器按键转换为 Mac 的 USB 鼠标、键盘和音量操作。Android Agent 在 Mac 上通过 Codex App Server 获取决策，调用 ADB 和 uiautomator2 操作手机，并读取真实界面验证任务结果。自主任务入口自动执行整个任务。

## 环境与安装

硬件验证使用 Waveshare ESP32-S3-Zero-N8R8、RC003-MS 遥控器、Apple Silicon Mac 和 Android 16 手机。Python 使用 3.11，Codex CLI 验证版本为 0.165.0。Codex CLI 需要已经登录并具有所选模型的使用权限。手机需要启用 USB 调试并授权 Mac。

在仓库根目录安装 Python 依赖：

```sh
mkdir -p work/runtime work/models work/deps
export TMPDIR="$PWD/work/runtime"
python3.11 -m venv work/venv
work/venv/bin/python -m pip install -r tools/android_agent/requirements.txt
work/venv/bin/python tools/android_agent/android_tools.py init --state-dir work/android-state
```

将 Android SDK Platform Tools 放在 `work/deps/platform-tools`，确保 `work/deps/platform-tools/adb devices` 能看到已授权设备。使用遥控器语音时，将 `mlx-community/whisper-large-v3-turbo` 模型下载至 `work/models/whisper-large-v3-turbo`，并允许运行 Python 的终端访问麦克风。

固件构建：

```sh
git submodule update --init vendor/tinyusb
pio run -e waveshare_s3_zero_n8r8
```

该环境使用 GPIO 21 控制 RGB，并固定 Arduino ESP32 与 TinyUSB 依赖版本。已有设备升级时，仅将 `.pio/build/waveshare_s3_zero_n8r8/firmware.bin` 写入应用分区 `0x10000`。分区表和 NVS 必须与当前设备一致。

## 启动

Android 自主任务使用实际手机序列号运行：

```sh
export ADBUTILS_ADB_PATH="$PWD/work/deps/platform-tools/adb"
work/venv/bin/python tools/android_agent/android_agent.py \
  --serial ANDROID_SERIAL --state-dir work/android-state \
  --work-dir work/phone-agent --allow-package com.android.settings \
  --task '打开手机设置，搜索显示' --output work/phone-result.json
```

这个入口自动启动本地工具服务、连接手机、获取模型决策、执行操作、验证结果并释放连接。任务未完成时会报错，操作记录保存在 `--output` 指定的文件中。

Android 16 手机可以加上 `--screen-off`，通过 `cmd display power-off 0` 关闭物理屏幕后操作。启动时手机需要已经解锁；密码锁定时接口返回 423，并停止界面读取和操作。息屏任务禁止调整音量、播放媒体和打开最近使用的应用。

模型通过当前界面的控件编号选择操作，再使用控件属性重新定位。文字不同、`resource_id` 相同的列表项可以分别选择。`scroll` 在所选列表内向上或向下滚动。每项决策保留此前实际观察到的界面和操作结果，单个任务最多执行 32 次决策。

Mac 鼠标在 `macos.13` 固件的 Mac 模式下直接通过 USB 使用，按键如下：

| 按键 | 操作 |
| --- | --- |
| 方向键 | 移动光标，按住逐渐加速，松开立即停止 |
| 中央确认键 | 鼠标左键；连续短按由 macOS 识别为双击 |
| 菜单键 | 鼠标右键 |
| 主页键 | 开始或结束左键拖动；方向键移动被拖动物体 |
| 电视键短按 | 切换光标移动与滚动；滚动模式下方向键分别控制横向和纵向滚动 |
| 语音键 | 按下和释放各发送一次 Control+Option+V |
| 音量键 | 系统音量增加或减少 |

拖动时显示橙色灯，滚动模式显示青色灯。切换滚动模式、断开遥控器或离开 Mac 模式会结束拖动。USB 鼠标报告直接使用 [Espressif Arduino ESP32 的 USBHIDMouse 与 USBHID](https://github.com/espressif/arduino-esp32/tree/2.0.17/libraries/USB/src)。按住加速及光标、滚轮分别计时的行为参考 [QMK Mouse Keys](https://docs.qmk.fm/features/mouse_keys)：光标每 20 ms 更新，由每次 1 个单位逐渐增加到 16 个单位；滚轮保持单步滚动，按住后缩短发送间隔。主页键锁定拖动适配遥控器的单键上报方式。

使用遥控器语音驱动手机任务时，启动串口服务：

用实际 ESP32 串口和手机序列号替换参数：

```sh
zsh tools/android_agent/run.zsh --port /dev/cu.usbmodemXXXX --serial ANDROID_SERIAL
```

默认进入 Mac 模式。`--mode android` 让方向、确认、返回、主页和音量按键控制手机；`--mode agent` 启用语音任务。一次只运行一个服务实例。

Mac 模式的语音按下和释放分别发送一次 `Control+Option+V`，微信输入法需要配置相同快捷键和 RemoteMapper 麦克风。Agent 模式按住麦克风键说话，松开后等待识别结果；黄色灯和语音提示表示需要按中央确认键。每项手机操作都需要确认，返回键取消当前任务。方向或主页按键接管任务并切换到 Android 模式。

工作期间紫色灯闪烁，录音期间为蓝色，完成后为绿色。程序退出或心跳中断超过五秒时，固件返回 Mac 模式。任务等待超过两分钟时取消。

默认允许 Agent 操作系统设置。需要其他应用时显式传入 `--allow-package`。工具服务在读取前检查前台应用，仅请求活动窗口，并保留允许应用的控件；通知栏内容不会发送给模型或写入任务记录。前台应用不在允许范围内时，界面记录包含空的控件列表。模型接收允许界面的控件文字、包名和任务历史；本地日志存放在 `work/`。模型通过当前 Codex 账号调用，使用量计入该账号。

GKD 与 uiautomator2 会竞争同一个 UiAutomation 连接。服务选择手机时会暂停正在运行的 GKD，释放手机时恢复 GKD。手机处于解锁状态时，会返回任务结束时的前台应用；已经锁定时保留锁屏。

## 接口与验证

本地工具服务只监听 `127.0.0.1:8765`，凭证位于 `work/android-state/token`，权限为当前用户可读写。`/v1/connect` 选择手机，`/v1/observe` 分页读取界面，`/v1/actions` 提交一次操作，`/v1/cancel` 作废未执行操作，`/v1/disconnect` 释放自动化连接。

控件操作绑定界面记录和连接代次。实体按键使用空的 `observation_id`。每次操作携带独立 `operation_id`，重复请求返回已记录结果；执行中断后的同一请求不会再次执行。手机断开或取消后，旧连接代次的请求失效。

固件串口提供 `events` 事件读取与确认、`agent` 任务指令、`status` 与 `usb_diag` 诊断。事件包含启动标识、事件流标识和递增序号；Mac 将事件写入 SQLite 后确认接收。队列溢出、序号不连续或设备重启会终止当前任务。实体确认由固件处理。

`check_firmware.py` 在真实 ESP32 上验证命令重复处理、实体确认要求、取消和心跳过期。运行时指定 `--port`、`--work-dir`。独立启动 `android_tools.py serve --state-dir work/android-state` 后，`check_service.py --state-dir work/android-state` 检查本地接口；`check_reliability.py` 需要额外提供 `--adb`、`--serial`、`--output`，会在真实手机上测试重复请求、取消和 ADB 重连。

`test/remote_pointer_test.c` 直接测试鼠标按键处理代码，覆盖精细移动、加速上限、按键释放、左右键、拖动、横向与纵向滚动、模式切换和时钟回绕。串口 `pointer` 返回滚动、拖动和 HID 发送状态。

GitHub Actions 的 `Mouse input verification` 在独立 Linux 虚拟机中使用 TinyUSB 鼠标描述符创建 UHID 设备，把同一份按键处理代码生成的报告交给内核，再通过 libevdev 验证实际输入事件。随后使用 LibFuzzer、AddressSanitizer 和 UndefinedBehaviorSanitizer 持续生成按键、松开、断连及时间变化。手动运行工作流时可以指定持续测试时间。

`Android tool verification` 在全新 Android 16 模拟器中验证接口鉴权、重复请求、取消、真实 ADB 重连、重复控件标识、滚动、文字替换与清空、应用范围限制、息屏操作、音量与媒体按键限制，以及密码锁定后拒绝读取和连接。`check_emulator.py` 会检查设备的模拟器标识。模拟输入验证与 ESP32、遥控器、Mac 的实体 USB 验证分别记录。

`annotations.py` 保存当前界面的文字备注。阅读页码或媒体时间需要调用方指定实际存在且唯一的控件标识；记录内容来自手机界面。
