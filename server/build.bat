@echo off

echo Building server
echo.

cargo build --release

if %ERRORLEVEL% == 0 (
    echo.
    echo Build succeeded.
) else (
    echo Build failed.
)

pause