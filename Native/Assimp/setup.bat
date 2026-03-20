@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo ============================================================
echo  Model Viewer - Windows Setup
echo ============================================================
echo.

:: ── Check 64-bit OS ───────────────────────────────────────────────────────────

if /i "%PROCESSOR_ARCHITECTURE%"=="x86" (
    if not defined PROCESSOR_ARCHITEW6432 (
        echo [ERROR] This setup requires a 64-bit version of Windows.
        pause & exit /b 1
    )
)
echo [OK] 64-bit OS confirmed

:: ── Check for winget ─────────────────────────────────────────────────────────

where winget >nul 2>&1
if errorlevel 1 (
    echo [ERROR] winget not found.
    echo   On Windows 11 it is built in. On Windows 10, install
    echo   "App Installer" from the Microsoft Store and re-run.
    pause & exit /b 1
)
echo [OK] winget found

:: ── Check for CMakeLists.txt ──────────────────────────────────────────────────

if not exist "%~dp0CMakeLists.txt" (
    echo [ERROR] CMakeLists.txt not found in script directory:
    echo   %~dp0
    echo   Please run this script from the project root folder.
    pause & exit /b 1
)
echo [OK] CMakeLists.txt found

:: ── Check for curl (with PowerShell fallback notice) ─────────────────────────

set DOWNLOAD_METHOD=curl
where curl >nul 2>&1
if errorlevel 1 (
    echo [WARN] curl not found - will use PowerShell for downloads.
    set DOWNLOAD_METHOD=powershell
)
echo [OK] Download method: %DOWNLOAD_METHOD%

:: ── Install MSYS2 via winget ──────────────────────────────────────────────────

echo.
echo [INFO] Installing MSYS2 (this may take a few minutes)...
winget install --id MSYS2.MSYS2 --accept-source-agreements --accept-package-agreements --silent
set WINGET_EXIT=%errorlevel%

:: winget returns 0 (success) or -1978335189 / 0x80073D02 (already installed).
:: Any other non-zero code is a genuine failure.
if %WINGET_EXIT% neq 0 (
    if %WINGET_EXIT% neq -1978335189 (
        echo [WARN] winget returned exit code %WINGET_EXIT%.
        echo   MSYS2 may already be installed. Checking for it now...
    ) else (
        echo [INFO] MSYS2 is already installed.
    )
)

:: ── Locate MSYS2 (check common locations) ────────────────────────────────────

set MSYS2_DIR=
for %%D in (C:\msys64 C:\msys2 D:\msys64 D:\msys2) do (
    if exist "%%D\usr\bin\bash.exe" (
        set MSYS2_DIR=%%D
    )
)

:: Also try the winget install location from registry as a fallback
if not defined MSYS2_DIR (
    for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MSYS2 64bit" /v InstallLocation 2^>nul') do (
        if exist "%%B\usr\bin\bash.exe" set MSYS2_DIR=%%B
    )
)

if not defined MSYS2_DIR (
    echo [ERROR] MSYS2 not found in any known location after install.
    echo   Checked: C:\msys64, C:\msys2, D:\msys64, D:\msys2, and registry.
    echo   Try installing manually from https://www.msys2.org/ then re-run.
    pause & exit /b 1
)
echo [OK] MSYS2 found at %MSYS2_DIR%

:: ── Install all dependencies via pacman ───────────────────────────────────────

echo.
echo [INFO] Updating pacman database and upgrading existing packages...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"
if errorlevel 1 (
    echo [ERROR] pacman -Syu failed.
    pause & exit /b 1
)

:: MSYS2 core packages sometimes require a second pass after the first Syu
echo [INFO] Running second pacman upgrade pass...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"
if errorlevel 1 (
    echo [ERROR] pacman second upgrade pass failed.
    pause & exit /b 1
)

echo [INFO] Installing compiler and libraries...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-glfw mingw-w64-x86_64-glm mingw-w64-x86_64-assimp"
if errorlevel 1 (
    echo [ERROR] pacman package install failed.
    pause & exit /b 1
)
echo [OK] Libraries installed

:: ── stb_image ─────────────────────────────────────────────────────────────────

if not exist "%~dp0stb_image.h" (
    echo.
    echo [INFO] Downloading stb_image.h...
    if "%DOWNLOAD_METHOD%"=="curl" (
        curl -sL "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" -o "%~dp0stb_image.h"
    ) else (
        powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nothings/stb/master/stb_image.h' -OutFile '%~dp0stb_image.h'"
    )
    if not exist "%~dp0stb_image.h" (
        echo [ERROR] Failed to download stb_image.h. Check your internet connection.
        pause & exit /b 1
    )
    echo [OK] stb_image.h downloaded
)

:: ── Build ─────────────────────────────────────────────────────────────────────

echo.
echo [INFO] Building...
echo.

:: Convert the Windows path to a POSIX path inside bash to safely handle spaces.
:: We pass the raw Windows path as an environment variable so it never touches
:: shell quoting at the cmd.exe level, then convert it with cygpath inside bash.
set "PROJECT_WIN_PATH=%~dp0"

%MSYS2_DIR%\usr\bin\bash.exe -lc ^
    "export PATH=/mingw64/bin:$PATH && ^
     SRC=$(cygpath \"$PROJECT_WIN_PATH\") && ^
     cd \"$SRC\" && ^
     rm -rf build && ^
     mkdir build && ^
     cd build && ^
     cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/mingw64 && ^
     ninja"
set BUILD_EXIT=%errorlevel%

if %BUILD_EXIT% neq 0 (
    echo.
    echo [ERROR] Build failed with exit code %BUILD_EXIT%. See output above.
    pause & exit /b 1
)

echo.
echo ============================================================
echo  Success^^!  Run model_viewer.exe to start.
echo ============================================================
pause