# ============================================================
# chipSetter 整体调度脚本
#
# 用法:
#   powershell -File scripts/workflow.ps1 <命令> [选项]
#
# 命令:
#   build       Mock 模式编译
#   build-real  Real GNC 模式编译
#   package     打包 (windeployqt + DLL → debug_deploy)
#   deploy      部署到 GNC (robocopy)
#   start       远端启动 (gdbserver + chipSetter)
#   stop        远端停止 (杀进程 + 删计划任务)
#   test        实机测试: build-real → package → deploy → start
#   debug       F5调试: build-real → package → deploy → start (同test)
#   quick       快速部署: 跳过编译, package → deploy → start
#   full        全流程: build → package (Mock)
#
# 选项:
#   -SkipBuild     跳过编译 (用于 test/debug 命令)
#   -SkipDeploy    跳过部署 (仅编译+打包)
#   -TargetIp      目标机IP (默认 192.168.1.2)
#   -Port          gdbserver 端口 (默认 1234)
# ============================================================
param(
    [Parameter(Position=0)]
    [ValidateSet("build","build-real","package","deploy","start","stop","test","debug","quick","full")]
    [string]$Command = "test",

    [switch]$SkipBuild,
    [switch]$SkipDeploy,
    [string]$TargetIp = "192.168.1.2",
    [int]$Port = 1234
)

$ErrorActionPreference = "Stop"

# ---- 路径配置 ----
$ProjectDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir  = Split-Path -Parent $ProjectDir
$QtPath      = "D:/tool/qt/5.15.2/mingw81_32"
$MSYS2Path   = "D:/Code_Languages/C/msys64"
$BashExe     = "$MSYS2Path/usr/bin/bash.exe"
$VscodeDir   = "$ProjectDir/.vscode"
$DebugDir    = "$ProjectDir/debug"
$DeployDir   = "$ProjectDir/debug_deploy"
$ExeName     = "chipSetter.exe"
$TargetShare = "\\$TargetIp\share\chipSetter"
$RemotePath  = "C:\Users\googol\Desktop\share\chipSetter"

# ---- 颜色 ----
function Write-Step($msg) { Write-Host "[$(Get-Date -Format HH:mm:ss)] $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "  ✅ $msg" -ForegroundColor Green }
function Write-Err($msg)  { Write-Host "  ❌ $msg" -ForegroundColor Red; throw $msg }
function Write-Warn($msg) { Write-Host "  ⚠️  $msg" -ForegroundColor Yellow }

# ============================================================
# Step 1: 编译
# ============================================================
function Invoke-Build {
    param([bool]$RealGnc = $false)
    Write-Step "编译 $(if($RealGnc){'Real GNC'}else{'Mock'}) 模式..."

    $qmakeArgs = "chipSetter.pro", "CONFIG+=debug", "CONFIG+=console"
    if ($RealGnc) { $qmakeArgs += "CONFIG+=real_gnc" }
    $qmakeArgsStr = $qmakeArgs -join " "

    $buildCmd = @(
        "export PATH=`$QT_PATH/bin:/mingw32/bin:`$PATH",
        "cd '$($ProjectDir -replace '\\','/')'",
        "`$QT_PATH/bin/qmake $qmakeArgsStr 2>&1",
        "make -f Makefile.Debug -j4 2>&1"
    ) -join " && "

    $proc = Start-Process -FilePath $BashExe `
        -ArgumentList "-lc", $buildCmd `
        -NoNewWindow -Wait -PassThru `
        -Environment @{
            MSYSTEM = "MINGW32"
            QT_PATH = $QtPath
        }

    if ($proc.ExitCode -ne 0) {
        Write-Err "编译失败 (exit=$($proc.ExitCode))"
    }

    $exe = "$DebugDir/$ExeName"
    if (-not (Test-Path $exe)) {
        Write-Err "编译产物缺失: $exe"
    }
    $size = [math]::Round((Get-Item $exe).Length / 1MB, 1)
    Write-OK "编译完成 ($size MB)"
}

# ============================================================
# Step 2: 打包
# ============================================================
function Invoke-Package {
    Write-Step "打包 → debug_deploy/..."

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/package_debug.ps1" 2>&1 | Out-Null

    if ($LASTEXITCODE -ne 0) {
        Write-Err "打包失败 (exit=$LASTEXITCODE)"
    }

    $count = (Get-ChildItem -Recurse $DeployDir -File).Count
    $size  = [math]::Round((Get-ChildItem -Recurse $DeployDir | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
    Write-OK "打包完成 ($count 文件, $size MB)"
}

# ============================================================
# Step 3: 部署
# ============================================================
function Invoke-Deploy {
    Write-Step "部署 → $TargetShare..."

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/deploy_to_target.ps1" 2>&1 | Select-Object -Last 5

    if ($LASTEXITCODE -ge 8) {
        Write-Err "部署失败 (robocopy exit=$LASTEXITCODE)"
    }
    Write-OK "部署完成 → GNC $RemotePath"
}

# ============================================================
# Step 4: 远端启动
# ============================================================
function Invoke-StartRemote {
    Write-Step "远端启动 (gdbserver → chipSetter)..."

    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/start_gdbserver.ps1" `
        -TargetIp $TargetIp -Port $Port 2>&1

    $resultString = $result -join "`n"

    if ($resultString -match "gdbserver listening") {
        Write-OK "gdbserver 监听 ${TargetIp}:$Port"
        Write-OK "chipSetter.exe 已拉起, 请查看 GNC 桌面"
    } elseif ($resultString -match "Cannot auto-start") {
        Write-Warn "自动启动失败, 请手动在 GNC 上运行:"
        Write-Host "    cd C:\Users\googol\Desktop\share\chipSetter" -ForegroundColor Cyan
        Write-Host "    gdbserver.exe --once 0.0.0.0:$Port chipSetter.exe" -ForegroundColor Cyan
    } else {
        Write-Warn "未知状态, 输出:"
        Write-Host $resultString
    }
}

# ============================================================
# Step 5: 远端停止
# ============================================================
function Invoke-StopRemote {
    Write-Step "远端停止..."

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/stop_gdbserver.ps1" 2>&1 | Select-Object -Last 3

    Write-OK "远端进程已停止"
}

# ============================================================
# 命令路由
# ============================================================
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  chipSetter Workflow — $Command"        -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$totalStart = Get-Date

switch ($Command) {

    "build" {
        Invoke-Build -RealGnc $false
    }

    "build-real" {
        Invoke-Build -RealGnc $true
    }

    "package" {
        Invoke-Package
    }

    "deploy" {
        Invoke-Deploy
    }

    "start" {
        Invoke-StartRemote
    }

    "stop" {
        Invoke-StopRemote
    }

    "test" {
        if (-not $SkipBuild) { Invoke-Build -RealGnc $true }
        Invoke-Package
        if (-not $SkipDeploy) { Invoke-Deploy }
        Invoke-StartRemote
    }

    "debug" {
        if (-not $SkipBuild) { Invoke-Build -RealGnc $true }
        Invoke-Package
        if (-not $SkipDeploy) { Invoke-Deploy }
        Invoke-StartRemote
    }

    "quick" {
        # 跳过编译, 直接打包→部署→启动
        Invoke-Package
        if (-not $SkipDeploy) { Invoke-Deploy }
        Invoke-StartRemote
    }

    "full" {
        if (-not $SkipBuild) { Invoke-Build -RealGnc $false }
        Invoke-Package
        Write-OK "Mock 全流程完成 (无部署)"
    }

    default {
        Write-Err "未知命令: $Command"
    }
}

$totalEnd = Get-Date
$elapsed = [math]::Round(($totalEnd - $totalStart).TotalSeconds, 1)
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  完成 ($elapsed 秒)"                    -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
