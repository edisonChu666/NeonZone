@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

rem 优先使用 PATH 中的 g++；也可用 W64DEVKIT / MINGW64 环境变量指向工具链根目录
where g++ >nul 2>nul
if errorlevel 1 if defined W64DEVKIT (
    set "PATH=%W64DEVKIT%\bin;%PATH%"
)
where g++ >nul 2>nul
if errorlevel 1 if defined MINGW64 (
    set "PATH=%MINGW64%\bin;%PATH%"
)
where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Install w64devkit / MinGW-w64 and add its bin to PATH,
    echo         or set W64DEVKIT / MINGW64 to the toolchain root directory.
    pause
    exit /b 1
)

echo [BUILD] Compiling NeonZone...
g++ -std=c++17 -O2 -municode -mwindows -static-libgcc -static-libstdc++ ^
    src/main.cpp ^
    src/core/Renderer.cpp ^
    src/core/Input.cpp ^
    src/core/Audio.cpp ^
    src/game/Entities.cpp ^
    src/game/Game.cpp ^
    -o NeonZone.exe -lwinmm

if errorlevel 1 (
    echo [FAILED] Compilation error.
    pause
    exit /b 1
)

echo [OK] NeonZone.exe built.
echo.
echo Run: NeonZone.exe
pause
