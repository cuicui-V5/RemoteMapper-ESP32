@echo off
chcp 65001 >nul
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\flash.ps1" %*
if %ERRORLEVEL% NEQ 0 (
    echo.
    pause
)