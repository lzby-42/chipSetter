# ============================================================
# chipSetter 项目 — Debug 编译脚本 (PowerShell 包装)
#
# 用法:
#   在项目根目录右键 "在终端中打开", 然后运行:
#     powershell -File scripts/build_debug.ps1
#
#   或带清理:
#     powershell -File scripts/build_debug.ps1 -Clean
#
# 环境要求:
#   - Qt 5.15.2 MinGW 32-bit
#   - MSYS2 with MINGW32 toolchain
# ============================================================
param(
    [switch]$Clean = $false
)

$ErrorActionPreference = "Stop"

# ---- 路径配置 ----
$QtPath    = "D:/tool/qt/5.15.2/mingw81_32"
$MSYS2Path = "D:/Code_Languages/C/msys64"
$BashExe   = "$MSYS2Path/usr/bin/bash.exe"

if (-not (Test-Path $BashExe)) {
    Write-Error "找不到 MSYS2 bash: $BashExe"
    exit 1
}

# ---- 切换到项目根目录 ----
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ProjectDir
Set-Location $ProjectDir

Write-Host "============================================"  -ForegroundColor Cyan
Write-Host " chipSetter — 编译 Debug 版本"               -ForegroundColor Cyan
Write-Host " 项目目录: $ProjectDir"                       -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# ---- 构建 MSYS2 命令 ----
$CleanArg = if ($Clean) { "clean" } else { "" }
$BashCmd  = "export MSYSTEM=MINGW32; export QT_PATH='$QtPath'; export MINGW_PATH=/mingw32; export PATH=`$QT_PATH/bin:/mingw32/bin:`$PATH; cd '$ProjectDir'; bash scripts/build_debug.sh $CleanArg 2>&1"

Write-Host "[0] 启动 MSYS2 MINGW32 编译环境..." -ForegroundColor Yellow

$proc = Start-Process -FilePath $BashExe `
    -ArgumentList "-lc", $BashCmd `
    -NoNewWindow -Wait -PassThru

if ($proc.ExitCode -ne 0) {
    Write-Host "编译失败! 退出码: $($proc.ExitCode)" -ForegroundColor Red
    exit $proc.ExitCode
}

Write-Host ""
Write-Host "编译成功! 可执行文件: debug/chipSetter.exe" -ForegroundColor Green
