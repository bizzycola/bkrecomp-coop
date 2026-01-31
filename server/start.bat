@echo off

echo Starting server...
echo.

if not exist "target\release\bkrecomp-coop-server.exe" (
    echo building...
    call build.bat
    if %ERRORLEVEL% neq 0 exit /b 1
)

echo.
echo Server starting...
echo Press Ctrl+C to stop
echo.

target\release\bkrecomp-coop-server.exe

pause
