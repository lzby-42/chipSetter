@echo off
REM ============================================================
REM chipSetter — 编译 Debug 版本 (Windows 批处理包装)
REM
REM 用法:
REM   双击运行, 或在项目根目录命令行中:
REM     scripts\build_debug.bat
REM
REM 带清理:
REM     scripts\build_debug.bat clean
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

"%BASH%" -lc "export MSYSTEM=MINGW32; export QT_PATH='%QT_PATH%'; export MINGW_PATH=/mingw32; export PATH=%QT_PATH%/bin:/mingw32/bin:$PATH; cd '%CD%'; bash scripts/build_debug.sh %CLEAN_ARG% 2>&1"

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
