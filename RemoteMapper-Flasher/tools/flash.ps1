# UTF-8 with BOM
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
$host.UI.RawUI.WindowTitle = "RemoteMapper-ESP32 一键免环境刷机工具"

$scriptDir = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path "$scriptDir\bin")) {
    $scriptDir = $PSScriptRoot
}
$binDir = "$scriptDir\bin"
$toolsDir = "$scriptDir\tools"
$esptool = "$toolsDir\esptool.exe"

function Show-Header {
    Clear-Host
    Write-Host "================================================================================" -ForegroundColor Cyan
    Write-Host "                  RemoteMapper-ESP32 固件免环境一键刷机工具" -ForegroundColor Green
    Write-Host "================================================================================" -ForegroundColor Cyan
    Write-Host "说明："
    Write-Host "  本工具已内置完整烧录环境与全型号预编译固件，电脑无需安装任何开发环境。"
    Write-Host "  支持 ESP32-S3 全系列硬件开发板（N16R8 / N8R2 / N4R2）。"
    Write-Host "================================================================================" -ForegroundColor Cyan
    Write-Host ""
}

function Wait-Enter {
    param([string]$Prompt = "按回车键继续...")
    Write-Host ""
    [void](Read-Host $Prompt)
}

# --- Step 1: 硬件物理连接 ---
Show-Header
Write-Host "【第 1 步 / 共 4 步】硬件物理连接" -ForegroundColor Magenta
Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "1. 请准备一根具备【数据传输功能】的 Type-C 数据线（切勿使用纯充电线）。"
Write-Host "2. 将数据线一端连接到 ESP32-S3 开发板的【USB / OTG】接口："
Write-Host "   ★ 提示：如果开发板上有两个 Type-C 接口（通常标有 COM/UART 和 USB），" -ForegroundColor Yellow
Write-Host "     请务必连接标有【USB】(Native USB) 的接口！" -ForegroundColor Yellow
Write-Host "3. 将另一端插入电脑的 USB 接口（推荐插在台式电脑机箱后置 USB 口）。"
Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
Wait-Enter "确认硬件连接就绪后，按回车键继续..."

# --- Step 2: 进入刷机模式 ---
Show-Header
Write-Host "【第 2 步 / 共 4 步】进入刷机模式 (Bootloader 模式)" -ForegroundColor Magenta
Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "为了确保芯片 100% 能够被烧录工具识别，请按照以下顺序操作开发板上的物理按键："
Write-Host ""
Write-Host "  【按键手势操作说明】：" -ForegroundColor Cyan
Write-Host "  +--------------------------------------------------------+" -ForegroundColor Cyan
Write-Host "  |  ① 用手指按住开发板上的【BOOT】按键，不要松开；       |" -ForegroundColor White
Write-Host "  |  ② 按一下开发板上的【RST】（或 EN / 重启）按键并松开； |" -ForegroundColor White
Write-Host "  |  ③ 松开【BOOT】按键。                                  |" -ForegroundColor White
Write-Host "  +--------------------------------------------------------+" -ForegroundColor Cyan
Write-Host ""
Write-Host "（完成上述三步后，ESP32-S3 芯片即已进入固件下载模式）" -ForegroundColor Green
Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
Wait-Enter "完成上述按键操作后，按回车键继续..."

# --- Step 3: 选择硬件规格 ---
function Select-Model {
    while ($true) {
        Show-Header
        Write-Host "【第 3 步 / 共 4 步】选择您的 ESP32-S3 硬件规格" -ForegroundColor Magenta
        Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
        Write-Host "请根据您购买的开发板型号选择对应固件（如果不清楚，普通开发板默认选 1）："
        Write-Host ""
        Write-Host "  [1] ESP32-S3-WROOM-1 N16R8 (16MB Flash, 8MB Octal PSRAM)  【官方推荐 / 最常用】" -ForegroundColor White
        Write-Host "  [2] ESP32-S3-WROOM-1 N8R2  (8MB Flash, 2MB Quad PSRAM)" -ForegroundColor White
        Write-Host "  [3] ESP32-S3-WROOM-1 N4R2  (4MB Flash, 2MB Quad PSRAM)" -ForegroundColor White
        Write-Host "  [Q] 退出程序" -ForegroundColor DarkGray
        Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
        Write-Host ""
        $choice = Read-Host "请输入编号 [1-3] (直接按回车默认为 1)"
        if ($null -eq $choice -or $choice.Trim() -eq "") {
            $choice = "1"
        } else {
            $choice = $choice.Trim()
        }

        if ($choice -ieq "Q") {
            Write-Host "`n感谢使用 RemoteMapper！"
            exit 0
        }
        if ($choice -eq "1") {
            return @{
                Name = "ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)"
                Bin = "RemoteMapper_ESP32S3_N16R8_full.bin"
            }
        }
        if ($choice -eq "2") {
            return @{
                Name = "ESP32-S3 N8R2 (8MB Flash, 2MB PSRAM)"
                Bin = "RemoteMapper_ESP32S3_N8R2_full.bin"
            }
        }
        if ($choice -eq "3") {
            return @{
                Name = "ESP32-S3 N4R2 (4MB Flash, 2MB PSRAM)"
                Bin = "RemoteMapper_ESP32S3_N4R2_full.bin"
            }
        }
        Write-Host "输入无效，请重新输入！" -ForegroundColor Red
        Start-Sleep -Seconds 1
    }
}

$model = Select-Model

# --- Step 4: 串口检测与选择 ---
function Select-Port {
    param($modelName)
    while ($true) {
        Show-Header
        Write-Host "【第 4 步 / 共 4 步】串口检测与确认" -ForegroundColor Magenta
        Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
        Write-Host "已选硬件规格: " -NoNewline
        Write-Host $modelName -ForegroundColor Green
        Write-Host "正在扫描电脑当前可用的串口设备，请稍候...`n"

        $ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Where-Object { $_ -match '^COM\d+$' } | Sort-Object -Unique)

        if ($null -eq $ports -or $ports.Count -eq 0) {
            Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
            Write-Host "【未检测到任何可用串口！】" -ForegroundColor Red
            Write-Host "排查建议："
            Write-Host "  1. 确认数据线是否具备数据传输功能（纯充电线无法被电脑识别）；"
            Write-Host "  2. 请确保已按 Step 2 执行 BOOT+RST 组合按键手势进入刷机模式；"
            Write-Host "  3. 尝试换一个电脑 USB 接口重新插拔一次；"
            Write-Host "  4. 打开 Windows 设备管理器，查看是否有黄色感叹号未知设备（需安装驱动）。"
            Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
            Write-Host "  [1] 重新扫描串口"
            Write-Host "  [2] 手动输入串口号 (例如 COM3)"
            Write-Host "  [Q] 退出程序"
            Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
            Write-Host ""
            $ans = Read-Host "请选择操作 [1/2/Q] (直接回车为重新扫描)"
            if ($null -eq $ans -or $ans.Trim() -eq "") { $ans = "1" } else { $ans = $ans.Trim() }
            if ($ans -ieq "Q") { exit 0 }
            if ($ans -eq "2") {
                $customPort = Read-Host "请输入串口号 (例如 COM3)"
                if ($null -ne $customPort) {
                    $customPort = $customPort.Trim().ToUpper()
                    if ($customPort -match '^\d+$') { $customPort = "COM$customPort" }
                    if ($customPort -ne "") { return $customPort }
                }
            }
            continue
        }

        for ($i = 0; $i -lt $ports.Count; $i++) {
            Write-Host "  [$($i + 1)] 发现可用串口: " -NoNewline
            Write-Host $ports[$i] -ForegroundColor Yellow
        }
        Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
        Write-Host ""

        $defPort = $ports[0]
        if ($ports.Count -eq 1) {
            Write-Host "检测到 1 个可用串口: " -NoNewline
            Write-Host "[$defPort]" -ForegroundColor Green
            $userPort = Read-Host "直接按回车确认使用 [$defPort]，或输入序号/端口号"
        } else {
            $defPort = $ports[-1]
            Write-Host "检测到多个可用串口设备。"
            $userPort = Read-Host "请输入要烧录的串口号或序号 (直接按回车默认使用 $defPort)"
        }

        if ([string]::IsNullOrWhiteSpace($userPort)) {
            return $defPort
        }
        $userPort = $userPort.Trim()
        if ($userPort -match '^\d+$') {
            $idx = [int]$userPort - 1
            if ($idx -ge 0 -and $idx -lt $ports.Count) {
                return $ports[$idx]
            }
        }
        if ($userPort -notmatch '^COM') {
            $userPort = "COM$userPort"
        }
        return $userPort.ToUpper()
    }
}

$port = Select-Port -modelName $model.Name

# --- Step 5: 烧录前确认与执行 ---
function Invoke-FlashProcess {
    param($model, $port)

    while ($true) {
        $binPath = "$binDir\$($model.Bin)"
        if (-not (Test-Path $binPath)) {
            Write-Host "`n[错误] 固件文件不存在: $binPath" -ForegroundColor Red
            Wait-Enter "按回车键退出..."
            exit 1
        }
        if (-not (Test-Path $esptool)) {
            Write-Host "`n[错误] 烧录工具不存在: $esptool" -ForegroundColor Red
            Wait-Enter "按回车键退出..."
            exit 1
        }

        Show-Header
        Write-Host "================================================================================" -ForegroundColor Cyan
        Write-Host "                           烧录参数确认" -ForegroundColor Yellow
        Write-Host "================================================================================" -ForegroundColor Cyan
        Write-Host "  目标芯片: " -NoNewline; Write-Host "ESP32-S3" -ForegroundColor White
        Write-Host "  硬件规格: " -NoNewline; Write-Host $model.Name -ForegroundColor White
        Write-Host "  固件镜像: " -NoNewline; Write-Host $model.Bin -ForegroundColor White
        Write-Host "  通信串口: " -NoNewline; Write-Host $port -ForegroundColor Green
        Write-Host "  烧录波特率: " -NoNewline; Write-Host "460800" -ForegroundColor White
        Write-Host "================================================================================" -ForegroundColor Cyan
        Wait-Enter "确认无误后，按回车键立即开始写入固件..."

        Write-Host "`n正在写入固件，全程通常需要 10~20 秒，请保持数据线连接..." -ForegroundColor Cyan
        Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray

        $flashArgs = @(
            "--chip", "esp32s3",
            "--port", $port,
            "--baud", "460800",
            "--before", "default_reset",
            "--after", "hard_reset",
            "write_flash",
            "--flash_mode", "keep",
            "--flash_freq", "keep",
            "--flash_size", "keep",
            "0x0", $binPath
        )

        & $esptool $flashArgs
        $flashResult = $LASTEXITCODE

        Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray

        if ($flashResult -eq 0) {
            Write-Host "`n================================================================================" -ForegroundColor Green
            Write-Host "                         ★ 固件烧录成功！★" -ForegroundColor Green
            Write-Host "================================================================================" -ForegroundColor Green
            Write-Host "【设备后续配置与使用指南】：" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "  1. 请按一下开发板上的【RST】按键（或重新插拔一次 USB 线），使设备正常启动；"
            Write-Host "  2. 打开手机或电脑的 WiFi 列表，搜索并连接热点："
            Write-Host "       WiFi 热点名称：RemoteMapper-AP" -ForegroundColor Cyan
            Write-Host "       初始连接密码：无（开放热点，直接点击连接）" -ForegroundColor Cyan
            Write-Host "  3. 连接成功后，打开浏览器访问控制后台："
            Write-Host "       http://192.168.4.1" -ForegroundColor Yellow
            Write-Host "  4. 在后台界面中，您可以："
            Write-Host "       - 扫描并绑定您的蓝牙遥控器；"
            Write-Host "       - 配置多层级自定义按键映射；"
            Write-Host "       - 在系统设置中为 WiFi AP 设置安全密码。"
            Write-Host "================================================================================" -ForegroundColor Green
            Write-Host ""
            Wait-Enter "刷机已全部完成，按回车键退出..."
            exit 0
        } else {
            Write-Host "`n================================================================================" -ForegroundColor Red
            Write-Host "                        ✖ 固件烧录失败！" -ForegroundColor Red
            Write-Host "================================================================================" -ForegroundColor Red
            Write-Host "常见失败原因与排查步骤："
            Write-Host ""
            Write-Host "  1. 【串口被占用】：请关闭所有可能占用串口的程序（如 Arduino IDE、串口助手等）；"
            Write-Host "  2. 【芯片未进入刷机模式】：请重新操作：按住【BOOT】键 -> 按一下【RST】键 -> 松开【BOOT】键；"
            Write-Host "  3. 【供电或数据线问题】：请将数据线直接插在电脑主机后置 USB 口；"
            Write-Host "  4. 【接口插错】：开发板若有两个 Type-C 接口，请务必插在 USB 口，不要插在 COM/UART 口。"
            Write-Host "================================================================================" -ForegroundColor Red
            Write-Host "  [1] 重新尝试烧录"
            Write-Host "  [2] 重新选择硬件型号与串口"
            Write-Host "  [Q] 退出程序"
            Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
            Write-Host ""
            $ans = Read-Host "请选择操作 [1/2/Q] (直接按回车重新尝试)"
            if ($null -eq $ans -or $ans.Trim() -eq "") { $ans = "1" } else { $ans = $ans.Trim() }
            if ($ans -ieq "Q") { exit 1 }
            if ($ans -eq "2") {
                $script:model = Select-Model
                $script:port = Select-Port -modelName $script:model.Name
            }
        }
    }
}

Invoke-FlashProcess -model $model -port $port