@echo off

set BUILD_DIR=%0\..\build
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
pushd %BUILD_DIR%

REM TODO eventually get rid of cstd?
REM Avoid /MD and /MT flags which link to the C runtime lib.
REM /NODEFAULTLIB - tells linker to avoid all default libraries
REM Look into /ENTRY and /SUBSYSTEM flags. We won't have the typical main() entry point which is
REM called by the C runtime (does it call the WinMain we have?)
REM /Z1 - omits default lib name from the object file

REM /Zi - include debug info
REM /Gm- - disable incremental compilation
REM /GR- - disable C++ runtime type information (can't use dynamic_cast or typeid checks)
REM /EHa- - disable async exception handling (specifically C++'s structured exception handling with try/catch)

REM WARNINGS
REM 4100: unreferenced formal parameter
REM 4101: unreferenced local variable

REM ==================================================
REM DEBUG BUILD
REM specifically for a release build we'd want to look at changing/removing the following:
REM     -FC -Od -Zi

set COMMON_COMPILER_FLAGS=-MTd -nologo -fp:fast -Gm- -GR- -EHa- -Fpermissive- -W4 -WX -wd4100 -wd4101 -Od -Oi -std:c++20 -Zi -FC
set COMMON_LINKER_FLAGS=-incremental:no -opt:ref

REM building the game as a dynamic library
cl ..\game.cpp -Fmgame.map -LD ^
    %COMMON_COMPILER_FLAGS% ^
    /link %COMMON_LINKER_FLAGS% -EXPORT:update_and_render

REM building the platform layer as an executable
cl ..\platform_win32.cpp -Fmplatform_win32.map ^
    -D_UNICODE ^
    %COMMON_COMPILER_FLAGS% ^
    /link %COMMON_LINKER_FLAGS% user32.lib gdi32.lib

popd
