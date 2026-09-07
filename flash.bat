@echo off
cd /d "%~dp0"

where platformio >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    python -m platformio --version >nul 2>nul
    if %ERRORLEVEL% NEQ 0 (
        echo [INFO] PlatformIO not detected. Launching standalone flasher...
        call "%~dp0RemoteMapper-Flasher\flash.bat"
        exit /b %ERRORLEVEL%
    )
)

set TARGET_ENV=esp32s3_n16r8
if "%1"=="n8r2" set TARGET_ENV=esp32s3_n8r2
if "%1"=="n4r2" set TARGET_ENV=esp32s3_n4r2
if "%1"=="n16r8" set TARGET_ENV=esp32s3_n16r8

echo ========================================================
echo  Flashing RemoteMapper-ESP32 [%TARGET_ENV%] to ESP32-S3
echo ========================================================
python -m platformio run -e %TARGET_ENV% -t upload
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Flash Failed!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] Flash completed successfully!
