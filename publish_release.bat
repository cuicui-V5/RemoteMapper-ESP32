@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ======================================================================
echo          RemoteMapper-ESP32 一键发布固件至 GitHub Releases
echo ======================================================================

:: 1. 检查 GitHub CLI (gh)
where gh >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 未检测到 GitHub CLI (gh.exe)！
    echo 请先安装并登录 gh (https://cli.github.com/)，运行 'gh auth login' 授权。
    pause
    exit /b 1
)

:: 2. 检查 7-Zip
set "SEVENZIP="
if exist "C:\Program Files\7-Zip\7z.exe" set "SEVENZIP=C:\Program Files\7-Zip\7z.exe"
if "%SEVENZIP%"=="" if exist "C:\Program Files (x86)\7-Zip\7z.exe" set "SEVENZIP=C:\Program Files (x86)\7-Zip\7z.exe"
if "%SEVENZIP%"=="" (
    where 7z >nul 2>nul
    if %ERRORLEVEL% EQU 0 set "SEVENZIP=7z"
)

if "%SEVENZIP%"=="" (
    echo [ERROR] 未找到 7-Zip 压缩工具！
    echo 请安装 7-Zip (https://www.7-zip.org/) 或将其路径添加至系统 PATH。
    pause
    exit /b 1
)

:: 3. 检查固件文件是否存在
if not exist "%~dp0RemoteMapper-Flasher\bin\RemoteMapper_ESP32S3_N16R8_full.bin" (
    echo [WARNING] 未检测到预编译固件！
    echo 正在自动调用 build.bat 进行全目标编译...
    echo.
    call "%~dp0build.bat" all
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] 固件编译失败，已终止发布流程！
        pause
        exit /b 1
    )
)

:: 4. 确定目标 Release Tag
set "TAG_NAME=%~1"
if "%TAG_NAME%"=="" set "TAG_NAME=release"

echo.
echo 目标 Release 标签: [%TAG_NAME%]
echo ----------------------------------------------------------------------
echo [1/2] 正在本地极速打包免环境刷机包...

:: 清理旧临时包并压缩为 .7z 与 .zip
del /f /q "%~dp0RemoteMapper-Flasher.7z" >nul 2>nul
del /f /q "%~dp0RemoteMapper-Flasher.zip" >nul 2>nul

"%SEVENZIP%" a -t7z -mx=9 "%~dp0RemoteMapper-Flasher.7z" "%~dp0RemoteMapper-Flasher" -y >nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 7z 打包失败！
    pause
    exit /b 1
)

"%SEVENZIP%" a -tzip -mx=9 "%~dp0RemoteMapper-Flasher.zip" "%~dp0RemoteMapper-Flasher" -y >nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] zip 打包失败！
    pause
    exit /b 1
)

echo [OK] 打包完成：
dir "%~dp0RemoteMapper-Flasher.7z" "%~dp0RemoteMapper-Flasher.zip" | findstr /i "RemoteMapper-Flasher"

echo.
echo ----------------------------------------------------------------------
echo [2/2] 正在直传附件至 GitHub Releases (无需等待 Actions 排队)...

:: 如果已有 Release 先删除旧 Release 记录（保留 git tag），确保附件全新上传
gh release view "%TAG_NAME%" >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo -> 检测到已有 Release [%TAG_NAME%]，正在刷新附件并同步发布...
    gh release delete "%TAG_NAME%" -y >nul 2>nul
)

set "NOTES_OPT="
if exist "%~dp0RemoteMapper-Flasher\使用说明.txt" (
    set NOTES_OPT=-F "%~dp0RemoteMapper-Flasher\使用说明.txt"
) else (
    set NOTES_OPT=-n "RemoteMapper-ESP32 最新构建免环境刷机包。"
)

gh release create "%TAG_NAME%" "%~dp0RemoteMapper-Flasher.7z" "%~dp0RemoteMapper-Flasher.zip" --title "免环境一键刷机包" !NOTES_OPT!

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 上传至 GitHub Releases 失败，请检查网络连接或权限！
    pause
    exit /b 1
)

echo.
echo ======================================================================
echo  [SUCCESS] 刷机包已成功直传并发布至 GitHub Releases！
echo  访问地址: https://github.com/cuicui-V5/RemoteMapper-ESP32/releases/tag/%TAG_NAME%
echo ======================================================================
echo.
pause
