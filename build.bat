@echo off

SET BUILD_DIR=%0\..\build
IF NOT EXIST %BUILD_DIR% mkdir %BUILD_DIR%

pushd %BUILD_DIR%

REM /Zi - include debug info
cl ..\platform_win32.cpp /Zi /link user32.lib
