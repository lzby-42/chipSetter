# ============================================================
#  chipSetter direct launch (no gdbserver) — for real-machine testing
#
#  Methods (tried in order):
#    1. PsExec -i  (precise, no scheduler delay)
#    2. WinRM + schtasks /it  (fallback, ~0-2min scheduler delay)
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

# ============================================================
# Method 1: PsExec (precise, no scheduler delay)
# ============================================================
function Start-ViaPsExec {
    Write-Host "[Method 1] Trying PsExec..." -ForegroundColor Yellow

    $psexec = $null
    $locations = @(
        "D:\tool\Sysinternals\PsExec.exe",
        "D:\Sysinternals\PsExec.exe",
        "PsExec.exe"
    )
    foreach ($loc in $locations) {
        if (Test-Path $loc) { $psexec = $loc; break }
    }
    if (-not $psexec) {
        Write-Host "[SKIP] PsExec not found." -ForegroundColor Yellow
        return $false
    }

    try {
        # Accept EULA (one-time per user)
        $null = & $psexec -accepteula 2>&1

        # Kill old, then launch directly via PsExec
        $cmd = "taskkill /f /im chipSetter.exe 2>nul & taskkill /f /im gdbserver.exe 2>nul & cd /d `"$RemoteExeDir`" & start `"`" chipSetter.exe"
        & $psexec "\\$TargetIp" -i -d cmd /c $cmd 2>&1 | Out-Null

        # Verify
        Start-Sleep -Seconds 2
        $credFile = "$env:USERPROFILE\.chipsetter_cred.xml"
        if (Test-Path $credFile) {
            $cred = Import-Clixml -Path $credFile
            $session = New-PSSession -ComputerName $TargetIp -Credential $cred -ErrorAction SilentlyContinue
            if ($session) {
                $alive = Invoke-Command -Session $session -ScriptBlock {
                    $p = Get-Process -Name 'chipSetter' -ErrorAction SilentlyContinue
                    if ($p) { Write-Output "PID=$($p.Id) Sess=$($p.SessionId) HWND=$($p.MainWindowHandle)" }
                }
                Remove-PSSession $session
                if ($alive) {
                    Write-Host "[OK] chipSetter running — $alive" -ForegroundColor Green
                    return $true
                }
            }
        }
        Write-Host "[WARN] Could not verify process (may still be starting)" -ForegroundColor Yellow
        return $true
    } catch {
        Write-Host "[SKIP] PsExec failed: $_" -ForegroundColor Yellow
        return $false
    }
}

# ============================================================
# Method 2: WinRM + schtasks /it (fallback)
# ============================================================
function Start-ViaSchtasks {
    Write-Host "[Method 2] Trying WinRM + schtasks..." -ForegroundColor Yellow

    $credFile = "$env:USERPROFILE\.chipsetter_cred.xml"
    if (-not (Test-Path $credFile)) {
        Write-Host "[SKIP] No credential" -ForegroundColor Yellow
        return $false
    }
    $cred = Import-Clixml -Path $credFile

    try {
        $session = New-PSSession -ComputerName $TargetIp -Credential $cred -ErrorAction Stop

        Invoke-Command -Session $session -ScriptBlock {
            param($dir)

            Get-Process -Name 'chipSetter','gdbserver' -ErrorAction SilentlyContinue | Stop-Process -Force
            $null = schtasks /delete /tn "chipSetter-direct" /f 2>&1
            Start-Sleep -Seconds 2

            $batch = "$env:TEMP\chipsetter_direct.bat"
            @"
@echo off
set "PATH=$dir;%PATH%"
cd /d "$dir"
start "chipSetter" chipSetter.exe
"@ | Out-File -Encoding ASCII -FilePath $batch

            $startTime = (Get-Date).AddMinutes(1).ToString("HH:mm:ss")
            $null = schtasks /create /tn "chipSetter-direct" /tr "cmd /c `"$batch`"" /sc once /st $startTime /it /f 2>&1
            Write-Host "Task scheduled, polling..."

            $chip = $null
            $deadline = (Get-Date).AddSeconds(120)
            while (-not $chip -and (Get-Date) -lt $deadline) {
                Start-Sleep -Seconds 3
                $chip = Get-Process -Name 'chipSetter' -ErrorAction SilentlyContinue
                if (-not $chip) { Write-Host -NoNewline "." }
            }
            Write-Host ""

            if ($chip) {
                $chip.Refresh()
                Write-Host "[OK] chipSetter PID=$($chip.Id)" -ForegroundColor Green
                if ($chip.MainWindowHandle -ne 0) {
                    Write-Host "     Window visible on GNC desktop" -ForegroundColor Green
                }
            } else {
                Write-Host "[FAIL] chipSetter not started within 120s" -ForegroundColor Red
            }

            $null = schtasks /delete /tn "chipSetter-direct" /f 2>&1

        } -ArgumentList $RemoteExeDir -ErrorAction Stop

        Remove-PSSession $session
        return $true

    } catch {
        Write-Host "[SKIP] WinRM/schtasks failed: $_" -ForegroundColor Yellow
        return $false
    }
}

# ============================================================
# Main
# ============================================================
$success = $false
if (-not $success) { $success = Start-ViaSchtasks }
if (-not $success) { $success = Start-ViaPsExec }

if (-not $success) {
    Write-Host "[FAIL] All methods failed." -ForegroundColor Red
    Write-Host "Manual: cd C:\Users\googol\Desktop\share\chipSetter && chipSetter.exe" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
