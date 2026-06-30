@echo off
REM ============================================================
REM chipSetter - Debug Build (GTS SDK)
REM
REM Usage:
REM   scripts\build_debug.bat         build
REM   scripts\build_debug.bat clean   clean + build
REM ============================================================
setlocal

set "QT_PATH=D:/tool/qt/5.15.2/mingw81_32"
set "MSYS2_PATH=D:/Code_Languages/C/msys64"
set "BASH=%MSYS2_PATH%\usr\bin\bash.exe"

if not exist "%BASH%" (
    echo [ERROR] MSYS2 bash not found: %BASH%
    pause
    exit /b 1
)

cd /d "%~dp0\.."

echo ============================================
echo  chipSetter - Build
echo ============================================

set "CLEAN_ARG="
if "%1"=="clean" set "CLEAN_ARG=clean"

set "SCRIPT_DIR=%CD:\=/%"
set "MSYSTEM=MINGW32"
set "QT_PATH_FWD=%QT_PATH:\=/%"

"%BASH%" -lc "export PATH=%QT_PATH_FWD%/bin:/mingw32/bin:$PATH; cd '%SCRIPT_DIR%'; bash scripts/build_debug.sh %CLEAN_ARG% 2>&1"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ============================================
    echo  Build FAILED!
    echo ============================================
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Build OK!  Output: debug\chipSetter.exe
echo ============================================
pause
