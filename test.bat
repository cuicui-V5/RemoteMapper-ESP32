@echo off
echo ========================================================
echo  Running RemoteMapper-ESP32 Algorithm and Unit Tests
echo ========================================================
set "PY=python"
if exist "%~dp0.venv\Scripts\python.exe" (
    set "PY=%~dp0.venv\Scripts\python.exe"
) else (
    where py >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        set "PY=py"
    )
)
%PY% test\native\test_suite.py
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Unit Tests Failed!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] All Unit Tests Passed!
