@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ======================================================================
echo          RemoteMapper-ESP32 Multi-Target Firmware Builder
echo ======================================================================

:: Determine esptool executable
set "ESPTOOL=%~dp0RemoteMapper-Flasher\tools\esptool.exe"
if not exist "%ESPTOOL%" (
    set "ESPTOOL=python -m esptool"
)

:: Locate boot_app0.bin
set "BOOT_APP0=%~dp0tools\boot_app0.bin"
if not exist "%BOOT_APP0%" (
    set "BOOT_APP0=%~dp0RemoteMapper-Flasher\tools\boot_app0.bin"
)
if not exist "%BOOT_APP0%" (
    echo [ERROR] boot_app0.bin not found in tools directory!
    exit /b 1
)

:: Ensure output directory exists
if not exist "%~dp0RemoteMapper-Flasher\bin" (
    mkdir "%~dp0RemoteMapper-Flasher\bin"
)

set "TARGET=%1"
if "%TARGET%"=="" set "TARGET=all"

if /i "%TARGET%"=="n16r8" goto :BUILD_N16R8
if /i "%TARGET%"=="esp32s3_n16r8" goto :BUILD_N16R8
if /i "%TARGET%"=="n8r2" goto :BUILD_N8R2
if /i "%TARGET%"=="esp32s3_n8r2" goto :BUILD_N8R2
if /i "%TARGET%"=="n4r2" goto :BUILD_N4R2
if /i "%TARGET%"=="esp32s3_n4r2" goto :BUILD_N4R2
if /i "%TARGET%"=="all" goto :BUILD_ALL

echo [ERROR] Unknown target: %TARGET%
echo Usage: build.bat [all | n16r8 | n8r2 | n4r2]
exit /b 1

:BUILD_ALL
echo [BUILD] Compiling all 3 firmware targets (N16R8, N8R2, N4R2)...
echo ----------------------------------------------------------------------
python -m platformio run -e esp32s3_n16r8 -e esp32s3_n8r2 -e esp32s3_n4r2
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ----------------------------------------------------------------------
echo [PACK] Merging binaries and copying to RemoteMapper-Flasher\bin...
echo ----------------------------------------------------------------------
call :MERGE_ONE esp32s3_n16r8 N16R8
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

call :MERGE_ONE esp32s3_n8r2 N8R2
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

call :MERGE_ONE esp32s3_n4r2 N4R2
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

goto :BUILD_SUMMARY

:BUILD_N16R8
echo [BUILD] Compiling esp32s3_n16r8...
python -m platformio run -e esp32s3_n16r8
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
call :MERGE_ONE esp32s3_n16r8 N16R8
goto :BUILD_SUMMARY

:BUILD_N8R2
echo [BUILD] Compiling esp32s3_n8r2...
python -m platformio run -e esp32s3_n8r2
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
call :MERGE_ONE esp32s3_n8r2 N8R2
goto :BUILD_SUMMARY

:BUILD_N4R2
echo [BUILD] Compiling esp32s3_n4r2...
python -m platformio run -e esp32s3_n4r2
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
call :MERGE_ONE esp32s3_n4r2 N4R2
goto :BUILD_SUMMARY

:MERGE_ONE
set "CUR_ENV=%~1"
set "CUR_TAG=%~2"
set "CUR_BUILD=%~dp0.pio\build\%CUR_ENV%"
set "CUR_FULL=%~dp0RemoteMapper-Flasher\bin\RemoteMapper_ESP32S3_%CUR_TAG%_full.bin"
set "CUR_APP=%~dp0RemoteMapper-Flasher\bin\RemoteMapper_ESP32S3_%CUR_TAG%_app.bin"

echo - Packaging %CUR_TAG% (0x0 full flash image)...
"%ESPTOOL%" --chip esp32s3 merge_bin -o "%CUR_FULL%" --flash_mode keep --flash_freq keep --flash_size keep 0x0 "%CUR_BUILD%\bootloader.bin" 0x8000 "%CUR_BUILD%\partitions.bin" 0xe000 "%BOOT_APP0%" 0x10000 "%CUR_BUILD%\firmware.bin"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to merge %CUR_TAG%!
    exit /b 1
)
copy /y "%CUR_BUILD%\firmware.bin" "%CUR_APP%" >nul
exit /b 0

:BUILD_SUMMARY
echo.
echo ======================================================================
echo  [SUCCESS] All targets built and copied to RemoteMapper-Flasher\bin!
echo ======================================================================
dir "%~dp0RemoteMapper-Flasher\bin\*.bin"
echo.
exit /b 0