@echo off
REM Fallout 2 CE — Windows Build Script
REM Builds the native Windows executable with CMake + MSVC or MinGW.
REM
REM Options (environment variables):
REM   PRESET          CMake preset name (default: windows-x64)
REM   CONFIG          Build configuration Debug|RelWithDebInfo (default: Debug)

if "%PRESET%"=="" set PRESET=windows-x64
if "%CONFIG%"=="" set CONFIG=Debug

REM Determine build preset name from configure preset + config.
REM The build preset naming convention is: {configure-preset}-{config-lower}
set BUILD_CONFIG=%CONFIG%
if /i "%CONFIG%"=="Debug"       set BUILD_CONFIG=debug
if /i "%CONFIG%"=="RelWithDebInfo" set BUILD_CONFIG=release

set BUILD_PRESET=%PRESET%-%BUILD_CONFIG%

echo [build-windows] Configure preset : %PRESET%
echo [build-windows] Build config     : %CONFIG%
echo [build-windows] Build preset     : %BUILD_PRESET%

REM Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [build-windows] ERROR: cmake not found. Install CMake >= 3.21.
    exit /b 1
)

REM Check cmake version >= 3.21 (required for cmake --build --preset)
for /f "tokens=3" %%v in ('cmake --version 2^>nul ^| findstr /i "cmake version"') do set CMAKE_VER=%%v
if not defined CMAKE_VER (
    echo [build-windows] ERROR: Could not determine cmake version.
    exit /b 1
)
echo [build-windows] CMake version: %CMAKE_VER%
for /f "tokens=1-3 delims=." %%a in ("%CMAKE_VER%") do (
    set CMAKE_MAJOR=%%a
    set CMAKE_MINOR=%%b
)
:: Use arithmetic to compare versions: major*1000 + minor vs 3021 threshold
set /A VER_OK=(%CMAKE_MAJOR%*1000+%CMAKE_MINOR%)-3021 2>nul
if %VER_OK% LSS 0 (
    echo [build-windows] ERROR: CMake >= 3.21 required. Found: %CMAKE_VER%
    exit /b 1
)

REM Detect Visual Studio environment.  If vswhere reports MSVC available,
REM use it; otherwise fall back to the preset's generator (Ninja for MinGW).
set USE_MSVC=0
if "%ProgramFiles(x86)%"=="" goto :no_vswhere
where vswhere >nul 2>&1
if %ERRORLEVEL% neq 0 goto :no_vswhere

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
    set VS_PATH=%%i
    set USE_MSVC=1
)

:no_vswhere

if "%USE_MSVC%"=="1" (
    echo [build-windows] Found Visual Studio at %VS_PATH%
    call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" ^
        && cmake --preset %PRESET% ^
        && cmake --build --preset %BUILD_PRESET%
) else (
    REM Bare CMake — relies on the preset's generator setting.
    echo [build-windows] Visual Studio not detected; using preset generator.
    cmake --preset %PRESET%
    if %ERRORLEVEL% neq 0 (
        echo [build-windows] ERROR: cmake configure failed.
        exit /b 1
    )
    cmake --build --preset %BUILD_PRESET%
)

if %ERRORLEVEL% neq 0 (
    echo [build-windows] Build FAILED.
    exit /b 1
)

echo [build-windows] Done — binary at out/build/%PRESET%/%CONFIG%/fallout2-ce.exe
exit /b 0
