@echo off

set SRC_DIR=%~dp0\src
set BUILD_DIR=%~dp0\build
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
pushd %BUILD_DIR%

REM TODO eventually get rid of cstd?
REM Avoid /MD and /MT flags which link to the C runtime lib.
REM /NODEFAULTLIB - tells linker to avoid all default libraries
REM Look into /ENTRY and /SUBSYSTEM flags. We won't have the typical main() entry point which is
REM called by the C runtime (does it call the WinMain we have?)
REM /Z1 - omits default lib name from the object file

REM WARNINGS
REM 4100: unreferenced formal parameter
REM 4101: unreferenced local variable
REM 4189: variable initialized but not referenced
REM 4172: conditional expression is constant

REM ==================================================
REM DEBUG BUILD
REM specifically for a release build we'd want to look at changing/removing the following:
REM     -FC -Od -Zi

set COMMON_COMPILER_FLAGS=^
    -MTd -nologo -std:c17 -fp:fast -Gm- -Od -Oi -Zi -FC^
    -W4 -WX -wd4100 -wd4101 -wd4189 -wd4127^
    -DASSERTIONS_ENABLED
set COMMON_LINKER_FLAGS=-incremental:no -opt:ref

REM building the game as a dynamic library
echo creating game dll lock file
echo game dll build in progress > game.lock
cl %SRC_DIR%\game.c -Fmgame.map -LD ^
    %COMMON_COMPILER_FLAGS% ^
    /link %COMMON_LINKER_FLAGS%
REM -EXPORT:game_update -EXPORT:game_render -EXPORT:game_init
del game.lock
echo game dll lock file deleted

REM building the platform layer as an executable
cl %SRC_DIR%\platform_win32.c -Fmplatform_win32.map ^
    -D_UNICODE ^
    %COMMON_COMPILER_FLAGS% ^
    /link %COMMON_LINKER_FLAGS% user32.lib gdi32.lib winmm.lib

popd
