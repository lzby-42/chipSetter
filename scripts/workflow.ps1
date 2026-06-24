# ============================================================
# chipSetter Workflow Script
#
# Usage:
#   powershell -File scripts/workflow.ps1 <command> [options]
#
# Commands:
#   build       Mock mode build
#   build-real  Real GNC mode build
#   package     Package (windeployqt + DLL -> debug_deploy)
#   deploy      Deploy to GNC (robocopy)
#   start       Remote start (gdbserver + chipSetter)
#   stop        Remote stop (kill process + delete scheduled task)
#   test        Real-machine test: build-real -> package -> deploy -> start
#   debug       F5 debug: same as test
#   quick       Quick deploy: skip build, package -> deploy -> start
#   full        Full flow: build -> package (Mock, no deploy)
#
# Options:
#   -SkipBuild     Skip build step (for test/debug)
#   -SkipDeploy    Skip deploy step (build+package only)
#   -TargetIp      Target IP (default 192.168.1.2)
#   -Port          gdbserver port (default 1234)
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

# ---- Paths ----
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

# ---- Helpers ----
function Write-Step($msg) { Write-Host "[$(Get-Date -Format HH:mm:ss)] $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "  [OK] $msg" -ForegroundColor Green }
function Write-Err($msg)  { Write-Host "  [ERR] $msg" -ForegroundColor Red; throw $msg }
function Write-Warn($msg) { Write-Host "  [WARN] $msg" -ForegroundColor Yellow }

# ============================================================
# Step 1: Build
# ============================================================
function Invoke-Build {
    param([bool]$RealGnc = $false)
    $mode = if ($RealGnc) { "Real GNC" } else { "Mock" }
    Write-Step "Building ($mode mode)..."

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
        Write-Err "Build failed (exit=$($proc.ExitCode))"
    }

    $exe = "$DebugDir/$ExeName"
    if (-not (Test-Path $exe)) {
        Write-Err "Build output missing: $exe"
    }
    $size = [math]::Round((Get-Item $exe).Length / 1MB, 1)
    Write-OK "Build complete ($size MB)"
}

# ============================================================
# Step 2: Package
# ============================================================
function Invoke-Package {
    Write-Step "Packaging -> debug_deploy/..."

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/package_debug.ps1" 2>&1 | Out-Null

    if ($LASTEXITCODE -ne 0) {
        Write-Err "Package failed (exit=$LASTEXITCODE)"
    }

    $count = (Get-ChildItem -Recurse $DeployDir -File).Count
    $size  = [math]::Round((Get-ChildItem -Recurse $DeployDir | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
    Write-OK "Package complete ($count files, $size MB)"
}

# ============================================================
# Step 3: Deploy
# ============================================================
function Invoke-Deploy {
    Write-Step "Deploying -> $TargetShare..."

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/deploy_to_target.ps1" 2>&1 | Select-Object -Last 5

    if ($LASTEXITCODE -ge 8) {
        Write-Err "Deploy failed (robocopy exit=$LASTEXITCODE)"
    }
    Write-OK "Deploy complete -> GNC $RemotePath"
}

# ============================================================
# Step 4: Remote Start
# ============================================================
function Invoke-StartRemote {
    Write-Step "Remote start (gdbserver -> chipSetter)..."

    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/start_gdbserver.ps1" `
        -TargetIp $TargetIp -Port $Port 2>&1

    $resultString = $result -join "`n"

    if ($resultString -match "gdbserver listening") {
        Write-OK "gdbserver listening on ${TargetIp}:$Port"
        Write-OK "chipSetter.exe launched, check GNC desktop"
    } elseif ($resultString -match "Cannot auto-start") {
        Write-Warn "Auto-start failed, run manually on GNC:"
        Write-Host "    cd C:\Users\googol\Desktop\share\chipSetter" -ForegroundColor Cyan
        Write-Host "    gdbserver.exe --once 0.0.0.0:$Port chipSetter.exe" -ForegroundColor Cyan
    } else {
        Write-Warn "Unknown status, output:"
        Write-Host $resultString
    }
}

# ============================================================
# Step 5: Remote Stop
# ============================================================
function Invoke-StopRemote {
    Write-Step "Remote stop..."

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File "$VscodeDir/stop_gdbserver.ps1" 2>&1 | Select-Object -Last 3

    Write-OK "Remote processes stopped"
}

# ============================================================
# Command Router
# ============================================================
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  chipSetter Workflow -- $Command"        -ForegroundColor Cyan
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
        Invoke-Package
        if (-not $SkipDeploy) { Invoke-Deploy }
        Invoke-StartRemote
    }

    "full" {
        if (-not $SkipBuild) { Invoke-Build -RealGnc $false }
        Invoke-Package
        Write-OK "Mock full flow complete (no deploy)"
    }

    default {
        Write-Err "Unknown command: $Command"
    }
}

$totalEnd = Get-Date
$elapsed = [math]::Round(($totalEnd - $totalStart).TotalSeconds, 1)
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Done ($elapsed sec)"                         -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
