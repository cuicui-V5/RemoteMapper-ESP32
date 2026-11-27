param(
    [string]$TagName = "release",
    [switch]$SkipBuild
)

# Set UTF-8 encoding
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$rootDir = Split-Path -Parent $PSScriptRoot
$flasherDir = Join-Path $rootDir "RemoteMapper-Flasher"
$binCheckFull = Join-Path $flasherDir "bin\RemoteMapper_ESP32S3_N16R8_full.bin"
$binCheckBoot = Join-Path $flasherDir "bin\RemoteMapper_ESP32S3_N16R8_bootloader.bin"
$sevenZipFile = Join-Path $rootDir "RemoteMapper-Flasher.7z"
$zipFile = Join-Path $rootDir "RemoteMapper-Flasher.zip"
$readmeInstructions = Join-Path $flasherDir "使用说明.txt"

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "         RemoteMapper-ESP32 一键发布固件至 GitHub Releases" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. 自动提取 Git 版本信息与提交说明
$gitHash = "unknown"
$gitMsg = "固件日常构建更新"
$gitDate = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")

$gitCmd = Get-Command git -ErrorAction SilentlyContinue
if ($gitCmd) {
    try {
        $gitHash = (& git -C $rootDir rev-parse --short HEAD).Trim()
        $gitMsg = ((& git -C $rootDir log -1 --pretty=%B) -join "`n").Trim()
        $gitDate = (& git -C $rootDir log -1 --pretty=%cd --date=format:"%Y-%m-%d %H:%M:%S").Trim()
    } catch {
        Write-Host "[WARN] 无法从 git 提取提交信息: $_" -ForegroundColor Yellow
    }
}

Write-Host "检测到 Git Commit: [$gitHash]" -ForegroundColor Magenta
Write-Host "构建发布时间戳:     [$gitDate]" -ForegroundColor Gray
Write-Host "本次更新提交说明:   [$gitMsg]" -ForegroundColor Gray
Write-Host ""

# 同步写入 include/version.h，确保当前与后续构建版本号完全一致
$versionHeader = @"
#pragma once

#define FIRMWARE_NAME       "RemoteMapper-ESP32"
#define FIRMWARE_VERSION    "$gitHash"
#define HARDWARE_TARGET     "ESP32-S3"
#define AUTHOR              "RemoteMapper Project"
"@
Set-Content -Path (Join-Path $rootDir "include\version.h") -Value $versionHeader -Encoding UTF8
Write-Host "[VERSION] 固件头文件版本号已自动同步为: $gitHash" -ForegroundColor Green

# 生成 version.json 供网页端刷机与硬件控制台在线检测
$versionJsonObj = [ordered]@{
    version = $gitHash
    commit = $gitHash
    commit_date = $gitDate
    commit_message = $gitMsg
    flash_url = "https://cuicui-v5.github.io/RemoteMapper-ESP32/flash.html"
}
$versionJsonStr = $versionJsonObj | ConvertTo-Json -Depth 4

$webFlasherDir = Join-Path $rootDir "web-flasher"
$webFlasherFirmwareDir = Join-Path $webFlasherDir "firmware"
if (-not (Test-Path -LiteralPath $webFlasherFirmwareDir)) {
    New-Item -ItemType Directory -Path $webFlasherFirmwareDir -Force | Out-Null
}
Set-Content -Path (Join-Path $webFlasherDir "version.json") -Value $versionJsonStr -Encoding UTF8
Set-Content -Path (Join-Path $webFlasherFirmwareDir "version.json") -Value $versionJsonStr -Encoding UTF8

# 2. 检查 GitHub CLI (gh)
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
    if (Test-Path -LiteralPath $p) {
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

# 3. 编译并打包最新固件
if (-not $SkipBuild) {
    Write-Host "[BUILD] 正在自动调用 build.bat 进行全目标最新固件编译与打包..." -ForegroundColor Cyan
    & "$rootDir\build.bat" all
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] 固件编译失败，已终止发布流程！" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "[BUILD] 跳过固件编译（使用现有本地 bin 固件）" -ForegroundColor Yellow
}

Write-Host "目标 Release 标签: [$TagName]" -ForegroundColor Magenta
Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[1/3] 正在本地极速打包免环境刷机包..." -ForegroundColor Cyan

# 清理旧临时文件
if (Test-Path -LiteralPath $sevenZipFile) { Remove-Item $sevenZipFile -Force }
if (Test-Path -LiteralPath $zipFile) { Remove-Item $zipFile -Force }

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
Write-Host "[2/3] 正在直传附件至 GitHub Releases (无需等待 Actions 排队)..." -ForegroundColor Cyan

# 检查已有 Release，如存在先删除 Release 记录（保留 git tag）以全新上传附件
$existingRelease = & gh release view $TagName 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "-> 检测到已有 Release [$TagName]，正在刷新附件并同步发布..." -ForegroundColor Yellow
    & gh release delete $TagName -y 2>&1 | Out-Null
}

$releaseTitle = "RemoteMapper-ESP32 固件发布 ($gitHash)"
$releaseNotes = @"
### 固件版本: ``$gitHash``
**构建发布时间**: $gitDate

#### 📝 本次更新内容 (Git Commit):
``````
$gitMsg
``````

#### ⚡ 刷机与使用通道:
- **网页端免驱动在线线刷 (推荐)**: https://cuicui-v5.github.io/RemoteMapper-ESP32/flash.html
- **离线绿色刷机包**: 下载下方附件中的 ``RemoteMapper-Flasher.7z`` 或 ``RemoteMapper-Flasher.zip``，解压后双击 ``一键烧录.bat`` 即可按向导极速刷机。

#### 📦 固件规格包含:
- 全型号 ESP32-S3 支持（N16R8 / N8R8 / N8R2 / N4R2）
- 包含全新刷机完整固件 (``*_full.bin``) 与保留配置升级固件 (``*_app.bin``)
"@

$tempNotesFile = Join-Path $env:TEMP ("remotemapper-notes-" + [System.Guid]::NewGuid().ToString().Substring(0,8) + ".md")
[System.IO.File]::WriteAllText($tempNotesFile, $releaseNotes, [System.Text.Encoding]::UTF8)

& gh release create $TagName $sevenZipFile $zipFile --title $releaseTitle --notes-file $tempNotesFile
if (Test-Path -LiteralPath $tempNotesFile) { Remove-Item $tempNotesFile -Force -ErrorAction SilentlyContinue }
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] 上传至 GitHub Releases 失败，请检查网络或权限！" -ForegroundColor Red
    exit 1
}

Write-Host "[OK] GitHub Releases 发布成功！" -ForegroundColor Green

Write-Host ""
Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[3/3] 正在发布 Web 网页端免驱动刷机工具至 GitHub Pages (gh-pages 分支)..." -ForegroundColor Cyan

$webFlasherDir = Join-Path $rootDir "web-flasher"
$ghPagesTempDir = Join-Path $env:TEMP ("remotemapper-ghpages-" + [System.Guid]::NewGuid().ToString().Substring(0,8))

try {
    New-Item -ItemType Directory -Path $ghPagesTempDir -Force | Out-Null
    Copy-Item "$webFlasherDir\index.html" "$ghPagesTempDir\" -Force
    if (Test-Path -LiteralPath "$webFlasherDir\flash.html") {
        Copy-Item "$webFlasherDir\flash.html" "$ghPagesTempDir\" -Force
    }
    if (Test-Path -LiteralPath "$webFlasherDir\i18n.js") {
        Copy-Item "$webFlasherDir\i18n.js" "$ghPagesTempDir\" -Force
    }
    Copy-Item "$webFlasherDir\esptool-bundle.js" "$ghPagesTempDir\" -Force
    if (Test-Path -LiteralPath "$webFlasherDir\robots.txt") {
        Copy-Item "$webFlasherDir\robots.txt" "$ghPagesTempDir\" -Force
    }
    if (Test-Path -LiteralPath "$webFlasherDir\sitemap.xml") {
        Copy-Item "$webFlasherDir\sitemap.xml" "$ghPagesTempDir\" -Force
    }
    if (Test-Path -LiteralPath "$webFlasherDir\version.json") {
        Copy-Item "$webFlasherDir\version.json" "$ghPagesTempDir\" -Force
    }

    $firmwareDestDir = Join-Path $ghPagesTempDir "firmware"
    New-Item -ItemType Directory -Path $firmwareDestDir -Force | Out-Null

    if (Test-Path -LiteralPath "$webFlasherDir\version.json") {
        Copy-Item "$webFlasherDir\version.json" "$firmwareDestDir\" -Force
    }

    $models = @("N16R8", "N8R8", "N8R2", "N4R2")
    foreach ($m in $models) {
        $fFull = Join-Path $flasherDir "bin\RemoteMapper_ESP32S3_${m}_full.bin"
        $fApp = Join-Path $flasherDir "bin\RemoteMapper_ESP32S3_${m}_app.bin"
        if (Test-Path -LiteralPath $fFull) { Copy-Item $fFull "$firmwareDestDir\" -Force }
        if (Test-Path -LiteralPath $fApp) { Copy-Item $fApp "$firmwareDestDir\" -Force }
    }

    New-Item -ItemType File -Path (Join-Path $ghPagesTempDir ".nojekyll") -Force | Out-Null

    $remoteOrigin = (& git -C $rootDir remote get-url origin).Trim()

    Push-Location $ghPagesTempDir
    & git init -q
    & git config user.name "github-actions[bot]"
    & git config user.email "github-actions[bot]@users.noreply.github.com"
    & git checkout -b gh-pages -q
    & git add .
    & git commit -m "Deploy Web Serial Flasher with firmware ($gitHash)" -q
    & git remote add origin $remoteOrigin
    & git push origin gh-pages --force
    Pop-Location

    Write-Host "[OK] GitHub Pages 部署成功！" -ForegroundColor Green
} catch {
    Write-Host "[WARNING] GitHub Pages 部署遇到问题: $_" -ForegroundColor Yellow
} finally {
    if (Test-Path -LiteralPath $ghPagesTempDir) {
        Remove-Item -Recurse -Force -LiteralPath $ghPagesTempDir -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Green
Write-Host " [SUCCESS] 固件发布与网页端刷机平台同步更新完成！" -ForegroundColor Green
Write-Host " Releases 下载: https://github.com/cuicui-V5/RemoteMapper-ESP32/releases/tag/$TagName" -ForegroundColor Cyan
Write-Host " 网页一键刷机: https://cuicui-v5.github.io/RemoteMapper-ESP32/" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Green
Write-Host ""
