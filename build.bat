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

REM debug build
REM specifically for a release build we'd want to look at changing/removing the following:
REM     -FC -Od -Zi
cl ..\platform_win32.cpp -Fmplatform_win32.map ^
    -D_UNICODE ^
    -MTd -nologo -fp:fast -Gm- -GR- -EHa- -Fpermissive -W4 -WX -wd4100 -Od -Oi -std:c++20 -Zi -FC ^
    /link -incremental:no -opt:ref user32.lib gdi32.lib

popd
