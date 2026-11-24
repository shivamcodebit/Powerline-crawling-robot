@echo off
title IIT Mandi Power Line Crawler Robot - Server
echo ==============================================================================
echo   IIT Mandi Power Line Inspection Crawler Robot - Windows Launcher
echo ==============================================================================
echo.

cd /d "%~dp0\.."

where python >nul 2>nul
if %ERRORLEVEL% equ 0 (
    python run_robot.py
) else (
    where py >nul 2>nul
    if %ERRORLEVEL% equ 0 (
        py -3 run_robot.py
    ) else (
        echo [ERROR] Python not found in system PATH.
        pause
    )
)
pause
