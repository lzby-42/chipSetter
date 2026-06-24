@echo off
REM ============================================================
REM chipSetter — 编译 Debug 版本 (GTS SDK)
REM
REM 用法:
REM   scripts\build_debug.bat         编译
REM   scripts\build_debug.bat clean   清理后编译
REM ============================================================
setlocal

set "QT_PATH=D:/tool/qt/5.15.2/mingw81_32"
set "MSYS2_PATH=D:/Code_Languages/C/msys64"
set "BASH=%MSYS2_PATH%\usr\bin\bash.exe"

if not exist "%BASH%" (
    echo [错误] 找不到 MSYS2 bash: %BASH%
    pause
    exit /b 1
)

cd /d "%~dp0\.."

echo ============================================
echo  chipSetter — 编译 Debug 版本
echo ============================================

set "CLEAN_ARG="
if "%1"=="clean" set "CLEAN_ARG=clean"

set "SCRIPT_DIR=%CD:\=/%"
set "MSYSTEM=MINGW32"
set "QT_PATH=%QT_PATH:\=/%"
"%BASH%" -lc "export PATH=%QT_PATH%/bin:/mingw32/bin:$PATH; cd '%SCRIPT_DIR%'; bash scripts/build_debug.sh %CLEAN_ARG% 2>&1"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ============================================
    echo  编译失败!
    echo ============================================
    pause
    exit /b 1
)

echo.
echo ============================================
echo  编译成功! 可执行文件: debug\chipSetter.exe
echo ============================================
pause
