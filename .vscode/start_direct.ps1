# ============================================================
#  chipSetter direct launch (no gdbserver) — for real-machine testing
#
#  Unlike start_gdbserver.ps1, this launches chipSetter.exe DIRECTLY
#  without gdbserver.  gdbserver suspends the process at startup and
#  waits for gdb to connect — without gdb, Qt never creates windows
#  (MainWindowHandle=0).  Use this script for testing; use
#  start_gdbserver.ps1 for F5 debugging.
# ============================================================
param(
    [string]$TargetIp = "192.168.1.2",
    [string]$RemoteUser = "192.168.1.2\googol",
    [string]$RemoteExeDir = "C:\Users\googol\Desktop\share\chipSetter"
)

$ErrorActionPreference = "Continue"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Direct Launch (no gdbserver)"          -ForegroundColor Cyan
Write-Host "  Target: $TargetIp"                      -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ---- Build credential ----
$credFile = "$env:USERPROFILE\.chipsetter_cred.xml"
$cred = $null

if (Test-Path $credFile) {
    try {
        $cred = Import-Clixml -Path $credFile
        Write-Host "[OK] Loaded credential" -ForegroundColor Green
    } catch {
        Write-Host "[ERROR] Credential file corrupted" -ForegroundColor Red
        exit 1
    }
}

if (-not $cred) {
    Write-Host "[ERROR] No credential found. Run start_gdbserver.ps1 first." -ForegroundColor Red
    exit 1
}

# ---- Launch via WinRM + schtasks ----
try {
    $session = New-PSSession -ComputerName $TargetIp -Credential $cred -ErrorAction Stop

    Invoke-Command -Session $session -ScriptBlock {
        param($dir)

        # Kill old instances
        Get-Process -Name 'chipSetter','gdbserver' -ErrorAction SilentlyContinue | Stop-Process -Force
        schtasks /delete /tn "chipSetter-direct" /f 2>$null
        schtasks /delete /tn "chipSetter-gdbserver" /f 2>$null
        Start-Sleep -Seconds 2

        # Write batch file that launches chipSetter directly
        $batch = "$env:TEMP\chipsetter_direct.bat"
        @"
@echo off
set "PATH=$dir;%PATH%"
cd /d "$dir"
start "chipSetter" chipSetter.exe
"@ | Out-File -Encoding ASCII -FilePath $batch

        Write-Host "Batch: $batch"
        Get-Content $batch | ForEach-Object { Write-Host "  $_" }

        # Schedule 10s in future, wait for scheduler to fire (NO /run — preserves /it)
        $startTime = (Get-Date).AddSeconds(30).ToString("HH:mm:ss")
        $taskName = "chipSetter-direct"
        $null = schtasks /create /tn $taskName /tr "cmd /c `"$batch`"" /sc once /st $startTime /it /f 2>&1
        Write-Host "Task scheduled at $startTime, waiting for scheduler..."
        Start-Sleep -Seconds 35

        # Check result
        $chip = Get-Process -Name 'chipSetter' -ErrorAction SilentlyContinue
        if ($chip) {
            Start-Sleep -Seconds 3
            $chip.Refresh()
            Write-Host "`n[OK] chipSetter PID=$($chip.Id) Session=$($chip.SessionId)" -ForegroundColor Green
            Write-Host "     Handle=$($chip.MainWindowHandle) Title='$($chip.MainWindowTitle)'"
            if ($chip.MainWindowHandle -ne 0) {
                Write-Host "     >>> Window visible on GNC desktop <<<" -ForegroundColor Green
            } else {
                Write-Host "     [WARN] No window handle (is user logged into GNC?)" -ForegroundColor Yellow
            }
        } else {
            Write-Host "[FAIL] chipSetter.exe did not start" -ForegroundColor Red
        }

        # Cleanup task
        schtasks /delete /tn $taskName /f 2>$null

    } -ArgumentList $RemoteExeDir -ErrorAction Stop

    Remove-PSSession $session

} catch {
    Write-Host "[ERROR] WinRM failed: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Manual launch on GNC (${TargetIp}):" -ForegroundColor Yellow
    Write-Host "  cd C:\Users\googol\Desktop\share\chipSetter" -ForegroundColor Cyan
    Write-Host "  chipSetter.exe" -ForegroundColor Cyan
    exit 1
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
