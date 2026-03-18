@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo ============================================================
echo  Model Viewer - Windows Setup (MSYS2/MinGW, no VS needed)
echo ============================================================
echo.

:: ── Check for MSYS2 ──────────────────────────────────────────────────────────

set MSYS2_DIR=C:\msys64

if not exist "%MSYS2_DIR%\usr\bin\bash.exe" (
    echo [ERROR] MSYS2 not found at %MSYS2_DIR%
    echo.
    echo  Please install MSYS2 first:
    echo    1. Go to https://www.msys2.org/
    echo    2. Download and run the installer
    echo    3. Use the default install path ^(C:\msys64^)
    echo    4. Re-run this script
    echo.
    pause & exit /b 1
)
echo [OK] MSYS2 found

:: ── Update package database first ────────────────────────────────────────────
echo.
echo [INFO] Updating package database...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -Sy --noconfirm"

:: ── Install packages ──────────────────────────────────────────────────────────
:: Notes on package names:
::   - git is a plain MSYS package (not mingw-prefixed)
::   - compiler, cmake, ninja, glfw, glm, assimp are all mingw-w64-x86_64-*
echo.
echo [INFO] Installing packages (may take a few minutes the first time)...
echo.

%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-glfw mingw-w64-x86_64-glm mingw-w64-x86_64-assimp"

if errorlevel 1 (
    echo [ERROR] Package install failed.
    echo   Open the MSYS2 MINGW64 terminal and run:
    echo     pacman -Syu
    echo   Then re-run this script.
    pause & exit /b 1
)
echo.
echo [OK] Packages installed

:: ── stb_image ─────────────────────────────────────────────────────────────────

if not exist "stb_image.h" (
    echo [INFO] Downloading stb_image.h...
    %MSYS2_DIR%\usr\bin\bash.exe -lc "curl -sL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o '/$(cygpath -u '%~dp0')stb_image.h' 2>/dev/null || curl -sL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o '%~dp0stb_image.h'"
    :: Simpler fallback: just use Windows curl directly
    if not exist "stb_image.h" (
        curl -sL "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" -o "stb_image.h"
    )
    if not exist "stb_image.h" (
        echo [ERROR] Failed to download stb_image.h
        pause & exit /b 1
    )
    echo [OK] stb_image.h downloaded
)

:: ── Build ─────────────────────────────────────────────────────────────────────
:: Convert the Windows source path to a Unix path that MSYS2 understands
echo.
echo [INFO] Building...
echo.

:: Use cygpath inside bash to safely convert the path
%MSYS2_DIR%\usr\bin\bash.exe -lc ^
    "export PATH=/mingw64/bin:$PATH && SRC=$(cygpath '%~dp0') && cd \"$SRC\" && rm -rf build && mkdir build && cd build && cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/mingw64 && ninja"

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed. See output above.
    echo   Try opening the MSYS2 MINGW64 terminal manually and running:
    echo     cd /your/project/path
    echo     mkdir build ^&^& cd build
    echo     cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/mingw64
    echo     ninja
    pause & exit /b 1
)

echo.
echo ============================================================
echo  Success^^!  Run model_viewer.exe to start.
echo ============================================================
pause
