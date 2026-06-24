@echo off
REM ============================================================
REM chipSetter — 编译真实硬件版本 (GTS SDK)
REM
REM 用法:
REM   双击运行, 或在项目根目录命令行中:
REM     scripts\build_deploy.bat
REM
REM 带清理:
REM     scripts\build_deploy.bat clean
REM
REM 输出: debug_deploy/chipSetter.exe + 所有依赖DLL
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
echo  chipSetter — 编译真实硬件版本 (GTS SDK)
echo ============================================

set "CLEAN_ARG="
if "%1"=="clean" set "CLEAN_ARG=clean"

set "SCRIPT_DIR=%CD:\=/%"
"%BASH%" -lc "export MSYSTEM=MINGW32; export QT_PATH='%QT_PATH%'; export PATH=%QT_PATH%/bin:/mingw32/bin:$PATH; cd '%SCRIPT_DIR%'; echo '[qmake] CONFIG+=real_gnc...'; %QT_PATH%/bin/qmake chipSetter.pro CONFIG+=debug CONFIG+=console CONFIG+=real_gnc; echo '[make]...'; make -f Makefile.Debug -j4 2>&1; echo '---'; ls -lh debug/chipSetter.exe 2>/dev/null"

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
echo  部署到目标机: 拷贝 debug\ 下所有文件
echo               到工控机即可运行
echo ============================================
pause
