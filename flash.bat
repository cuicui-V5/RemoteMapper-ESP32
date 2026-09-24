@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

:: Determine PlatformIO runner
set "PIO=pio"
where pio >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    where platformio >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        set "PIO=platformio"
    ) else (
        echo [INFO] PlatformIO CLI not found in PATH. Launching standalone flasher...
        call "%~dp0RemoteMapper-Flasher\flash.bat"
        exit /b %ERRORLEVEL%
    )
)

:: Parse arguments
set "TARGET_ENV="
set "UPLOAD_PORT="

call :PARSE_ARG "%~1"
call :PARSE_ARG "%~2"

if "%TARGET_ENV%"=="" set "TARGET_ENV=esp32s3_n16r8"

:: Auto-detect active serial port if not manually specified
if "%UPLOAD_PORT%"=="" (
    for /f "tokens=*" %%i in ('powershell.exe -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames()"') do (
        if "%UPLOAD_PORT%"=="" set "UPLOAD_PORT=%%i"
    )
)

set "PORT_FLAG="
if not "%UPLOAD_PORT%"=="" (
    set "PORT_FLAG=--upload-port %UPLOAD_PORT%"
    echo [INFO] Target COM Port: %UPLOAD_PORT%
) else (
    echo [WARN] No active COM port detected. Will attempt PlatformIO default scan...
)

echo ========================================================
echo  Flashing RemoteMapper-ESP32 [%TARGET_ENV%] to ESP32-S3
echo ========================================================

%PIO% run -e %TARGET_ENV% -t upload %PORT_FLAG%
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================================
    echo  [ERROR] Flash Failed!
    echo ========================================================
    echo  Troubleshooting tips:
    echo   1. Ensure ESP32-S3 is connected via data-capable USB cable.
    echo   2. Put chip into Bootloader mode: Hold BOOT button -^> Click RST -^> Release BOOT.
    echo   3. Specify port manually: .\flash.bat %TARGET_ENV% COMx
    echo   4. Or run standalone flasher: .\RemoteMapper-Flasher\flash.bat
    echo.
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  [SUCCESS] Flash completed successfully!
echo ========================================================
exit /b 0

:PARSE_ARG
if "%~1"=="" exit /b 0
echo %~1 | findstr /i "^COM[0-9]" >nul && (
    set "UPLOAD_PORT=%~1"
    exit /b 0
)
if /i "%~1"=="n16r8" set "TARGET_ENV=esp32s3_n16r8" & exit /b 0
if /i "%~1"=="esp32s3_n16r8" set "TARGET_ENV=esp32s3_n16r8" & exit /b 0
if /i "%~1"=="n8r8" set "TARGET_ENV=esp32s3_n8r8" & exit /b 0
if /i "%~1"=="esp32s3_n8r8" set "TARGET_ENV=esp32s3_n8r8" & exit /b 0
if /i "%~1"=="n8r2" set "TARGET_ENV=esp32s3_n8r2" & exit /b 0
if /i "%~1"=="esp32s3_n8r2" set "TARGET_ENV=esp32s3_n8r2" & exit /b 0
if /i "%~1"=="n4r2" set "TARGET_ENV=esp32s3_n4r2" & exit /b 0
if /i "%~1"=="esp32s3_n4r2" set "TARGET_ENV=esp32s3_n4r2" & exit /b 0
exit /b 0
