param(
    [string]$TagName = "release"
)

# Set UTF-8 encoding
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$rootDir = Split-Path -Parent $PSScriptRoot
$flasherDir = Join-Path $rootDir "RemoteMapper-Flasher"
$binCheck = Join-Path $flasherDir "bin\RemoteMapper_ESP32S3_N16R8_full.bin"
$sevenZipFile = Join-Path $rootDir "RemoteMapper-Flasher.7z"
$zipFile = Join-Path $rootDir "RemoteMapper-Flasher.zip"
$readmeInstructions = Join-Path $flasherDir "使用说明.txt"

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "         RemoteMapper-ESP32 一键发布固件至 GitHub Releases" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. 检查 GitHub CLI (gh)
$ghCmd = Get-Command gh -ErrorAction SilentlyContinue
if (-not $ghCmd) {
    Write-Host "[ERROR] 未检测到 GitHub CLI (gh.exe)！" -ForegroundColor Red
    Write-Host "请先安装并登录 gh (https://cli.github.com/)，运行 'gh auth login' 授权。" -ForegroundColor Yellow
    exit 1
}

# 2. 检查 7-Zip
$sevenZip = $null
$possiblePaths = @(
    "C:\Program Files\7-Zip\7z.exe",
    "C:\Program Files (x86)\7-Zip\7z.exe"
)
foreach ($p in $possiblePaths) {
    if (Test-Path $p) {
        $sevenZip = $p
        break
    }
}
if (-not $sevenZip) {
    $sevenZipCmd = Get-Command 7z -ErrorAction SilentlyContinue
    if ($sevenZipCmd) {
        $sevenZip = "7z"
    }
}
if (-not $sevenZip) {
    Write-Host "[ERROR] 未检测到 7-Zip 压缩工具！" -ForegroundColor Red
    Write-Host "请先安装 7-Zip (https://www.7-zip.org/) 或将其路径添加至系统 PATH。" -ForegroundColor Yellow
    exit 1
}

# 3. 检查固件是否存在
if (-not (Test-Path $binCheck)) {
    Write-Host "[WARNING] 未检测到预编译固件！正在自动调用 build.bat 进行全目标编译..." -ForegroundColor Yellow
    & "$rootDir\build.bat" all
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] 固件编译失败，已终止发布流程！" -ForegroundColor Red
        exit 1
    }
}

Write-Host "目标 Release 标签: [$TagName]" -ForegroundColor Magenta
Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[1/2] 正在本地极速打包免环境刷机包..." -ForegroundColor Cyan

# 清理旧临时文件
if (Test-Path $sevenZipFile) { Remove-Item $sevenZipFile -Force }
if (Test-Path $zipFile) { Remove-Item $zipFile -Force }

# 压缩为 .7z 与 .zip
& $sevenZip a -t7z -mx=9 $sevenZipFile $flasherDir -y | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] 7z 打包失败！" -ForegroundColor Red
    exit 1
}

& $sevenZip a -tzip -mx=9 $zipFile $flasherDir -y | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] zip 打包失败！" -ForegroundColor Red
    exit 1
}

$sz7 = [math]::Round((Get-Item $sevenZipFile).Length / 1MB, 2)
$szZip = [math]::Round((Get-Item $zipFile).Length / 1MB, 2)
Write-Host "[OK] 打包完成：" -ForegroundColor Green
Write-Host "  - 7z  归档: $sevenZipFile ($sz7 MB)" -ForegroundColor Gray
Write-Host "  - ZIP 归档: $zipFile ($szZip MB)" -ForegroundColor Gray

Write-Host ""
Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[2/2] 正在直传附件至 GitHub Releases (无需等待 Actions 排队)..." -ForegroundColor Cyan

# 检查已有 Release，如存在先删除 Release 记录（保留 git tag）以全新上传附件
$existingRelease = & gh release view $TagName 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "-> 检测到已有 Release [$TagName]，正在刷新附件并同步发布..." -ForegroundColor Yellow
    & gh release delete $TagName -y 2>&1 | Out-Null
}

$notesParam = @()
if (Test-Path $readmeInstructions) {
    $notesParam = @("-F", $readmeInstructions)
} else {
    $notesParam = @("-n", "RemoteMapper-ESP32 最新构建免环境刷机包。")
}

& gh release create $TagName $sevenZipFile $zipFile --title "免环境一键刷机包" @notesParam
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] 上传至 GitHub Releases 失败，请检查网络或权限！" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Green
Write-Host " [SUCCESS] 刷机包已成功直传并发布至 GitHub Releases！" -ForegroundColor Green
Write-Host " 访问地址: https://github.com/cuicui-V5/RemoteMapper-ESP32/releases/tag/$TagName" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Green
Write-Host ""
