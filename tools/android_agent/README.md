# Mac 与 Android 遥控服务

ESP32 保存任务状态、按键队列和操作确认状态。Mac 从 RemoteMapper USB 麦克风录音，使用本机 Whisper 模型识别语音，通过 Codex App Server 获取下一步操作，再通过 ADB 和 uiautomator2 操作 Android 手机。

## 环境与安装

硬件验证使用 Waveshare ESP32-S3-Zero-N8R8、RC003-MS 遥控器、Apple Silicon Mac 和 Android 16 手机。Python 使用 3.11，Codex CLI 验证版本为 0.165.0。Codex CLI 需要已经登录并具有所选模型的使用权限。手机需要启用 USB 调试并授权 Mac。

在仓库根目录安装 Python 依赖：

```sh
mkdir -p work/runtime work/models work/deps
export TMPDIR="$PWD/work/runtime"
python3.11 -m venv work/venv
work/venv/bin/python -m pip install -r tools/android_agent/requirements.txt
```

将 Android SDK Platform Tools 放在 `work/deps/platform-tools`，确保 `work/deps/platform-tools/adb devices` 能看到已授权设备。将 `mlx-community/whisper-large-v3-turbo` 模型下载至 `work/models/whisper-large-v3-turbo`。首次使用需要允许运行 Python 的终端访问麦克风。

固件构建：

```sh
git submodule update --init vendor/tinyusb
pio run -e waveshare_s3_zero_n8r8
```

该环境使用 GPIO 21 控制 RGB，并固定 Arduino ESP32 与 TinyUSB 依赖版本。已有设备升级时，仅将 `.pio/build/waveshare_s3_zero_n8r8/firmware.bin` 写入应用分区 `0x10000`。分区表和 NVS 必须与当前设备一致。

## 启动

用实际 ESP32 串口和手机序列号替换参数：

```sh
zsh tools/android_agent/run.zsh --port /dev/cu.usbmodemXXXX --serial ANDROID_SERIAL
```

默认进入 Mac 模式。`--mode android` 让方向、确认、返回、主页和音量按键控制手机；`--mode agent` 启用语音任务。一次只运行一个服务实例。

Mac 模式的语音按下和释放分别发送一次 `Control+Option+V`，微信输入法需要配置相同快捷键和 RemoteMapper 麦克风。Agent 模式按住麦克风键说话，松开后等待识别结果；黄色灯和语音提示表示需要按中央确认键。每项手机操作都需要确认，返回键取消当前任务。方向或主页按键接管任务并切换到 Android 模式。

工作期间紫色灯闪烁，录音期间为蓝色，完成后为绿色。程序退出或心跳中断超过五秒时，固件返回 Mac 模式。任务等待超过两分钟时取消。

默认允许 Agent 操作系统设置。需要其他应用时显式传入 `--allow-package`。模型接收当前界面的控件文字、包名和任务历史；本地日志也会保存这些信息，存放在 `work/`。模型通过当前 Codex 账号调用，使用量计入该账号。

GKD 与 uiautomator2 会竞争同一个 UiAutomation 连接。服务选择手机时会暂停正在运行的 GKD，释放手机时重新打开 GKD；这个恢复动作会将 GKD 带到前台。

## 接口与验证

本地工具服务只监听 `127.0.0.1:8765`，凭证位于 `work/android-state/token`，权限为当前用户可读写。`/v1/connect` 选择手机，`/v1/observe` 分页读取界面，`/v1/actions` 提交一次操作，`/v1/cancel` 作废未执行操作，`/v1/disconnect` 释放自动化连接。

控件操作绑定界面记录和连接代次。实体按键使用空的 `observation_id`。每次操作携带独立 `operation_id`，重复请求返回已记录结果；执行中断后的同一请求不会再次执行。手机断开或取消后，旧连接代次的请求失效。

固件串口提供 `events` 事件读取与确认、`agent` 任务指令、`status` 与 `usb_diag` 诊断。事件包含启动标识、事件流标识和递增序号；Mac 将事件写入 SQLite 后确认接收。队列溢出、序号不连续或设备重启会终止当前任务。实体确认由固件处理。

`check_firmware.py` 在真实 ESP32 上验证命令重复处理、实体确认要求、取消和心跳过期。运行时指定 `--port`、`--work-dir`。独立启动 `android_tools.py serve --state-dir work/android-state` 后，`check_service.py --state-dir work/android-state` 检查本地接口；`check_reliability.py` 需要额外提供 `--adb`、`--serial`、`--output`，会在真实手机上测试重复请求、取消和 ADB 重连。

`annotations.py` 保存当前界面的文字备注。阅读页码或媒体时间需要调用方指定实际存在且唯一的控件标识；记录内容来自手机界面。
