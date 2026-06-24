param(
    [string]$TargetIp = "192.168.1.2",
    [int]$Port = 1234,
    [string]$RemoteExePath = "C:\Users\googol\Desktop\share\chipSetter\chipSetter.exe",
    [string]$RemoteGdbServerPath = "C:\Users\googol\Desktop\share\chipSetter\gdbserver.exe",
    [string]$RemoteUser = "192.168.1.2\googol"
)

$ErrorActionPreference = "Continue"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Start gdbserver on GNC target" -ForegroundColor Cyan
Write-Host "  Target: ${TargetIp}:${Port}" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ---- Build credential ----
$credFile = "$env:USERPROFILE\.chipsetter_cred.xml"
$cred = $null

if (Test-Path $credFile) {
    try {
        $cred = Import-Clixml -Path $credFile
        Write-Host "[OK] Loaded saved credential for $RemoteUser" -ForegroundColor Green
    } catch {
        Write-Host "[WARN] Credential file corrupted, will re-create." -ForegroundColor Yellow
        Remove-Item $credFile -Force -ErrorAction SilentlyContinue
    }
}

if (-not $cred) {
    Write-Host "[INFO] First time: enter GNC password (saved encrypted)" -ForegroundColor Yellow
    $securePass = Read-Host "Password for $RemoteUser" -AsSecureString
    if ($securePass) {
        $cred = New-Object System.Management.Automation.PSCredential($RemoteUser, $securePass)
        $cred | Export-Clixml -Path $credFile
        Write-Host "[OK] Credential saved to $credFile" -ForegroundColor Green
    }
}

# ---- Method 1: PowerShell Remoting (WinRM) ----
function Start-ViaWinRM {
    Write-Host "[Method 1] Trying WinRM..." -ForegroundColor Yellow
    $session = $null
    try {
        $splat = @{ ComputerName = $TargetIp; ErrorAction = "Stop" }
        if ($cred) { $splat.Credential = $cred } else { throw "No credential available" }
        $session = New-PSSession @splat
        Invoke-Command -Session $session -ScriptBlock {
            param($exe, $gdbserver, $port)
            Get-Process -Name "chipSetter" -ErrorAction SilentlyContinue | Stop-Process -Force
            Get-Process -Name "gdbserver" -ErrorAction SilentlyContinue | Stop-Process -Force
            Start-Sleep -Seconds 1

            $taskName = "chipSetter-gdbserver"
            $dir = Split-Path $exe -Parent

            # Write a batch file to the remote machine
            $batchPath = "$env:TEMP\chipsetter_gdb.bat"
            @"
@echo off
set "PATH=$dir;%PATH%"
cd /d "$dir"
gdbserver.exe --once 0.0.0.0:$port chipSetter.exe 1> "%TEMP%\gdb_stdout.txt" 2> "%TEMP%\gdb_stderr.txt"
"@ | Out-File -Encoding ASCII -FilePath $batchPath

            # Debug: show batch content
            Write-Host "Batch: $batchPath"
            Get-Content $batchPath | ForEach-Object { Write-Host "  $_" }

            $startTime = (Get-Date).AddMinutes(1).ToString("HH:mm")
            schtasks /delete /tn $taskName /f 2>$null
            $createOut = schtasks /create /tn $taskName /tr "cmd /c `"$batchPath`"" /sc once /st $startTime /it /f 2>&1
            Write-Host "SchTasks Create: $createOut"
            $runOut = schtasks /run /tn $taskName 2>&1
            Write-Host "SchTasks Run: $runOut"
            Start-Sleep -Seconds 5

            # Check log files for error messages
            @("$env:TEMP\gdb_stdout.txt", "$env:TEMP\gdb_stderr.txt") | ForEach-Object {
                if (Test-Path $_) {
                    Write-Host "$(Split-Path $_ -Leaf):"
                    $content = Get-Content $_ -Raw
                    if ($content) { Write-Host $content } else { Write-Host "  (empty)" }
                }
            }

            $portCheck = (netstat -ano | Select-String "0.0.0.0:$port.*LISTENING")
            if ($portCheck) {
                Write-Host "[OK] gdbserver listening on $port" -ForegroundColor Green
            } else {
                Write-Host "[WARN] Port $port not listening" -ForegroundColor Yellow
            }
        } -ArgumentList $RemoteExePath, $RemoteGdbServerPath, $Port -ErrorAction Stop
        Write-Host "[OK] gdbserver started via WinRM" -ForegroundColor Green
        Remove-PSSession $session
        return $true
    } catch {
        Write-Host "[SKIP] WinRM failed: $_" -ForegroundColor Yellow
        if ($session) { Remove-PSSession $session -ErrorAction SilentlyContinue }
        return $false
    }
}

# ---- Method 2: PsExec ----
function Start-ViaPsExec {
    Write-Host "[Method 2] Trying PsExec..." -ForegroundColor Yellow
    $psexec = $null
    $locations = @(
        "D:\Sysinternals\PsExec.exe",
        "C:\Sysinternals\PsExec.exe",
        "PsExec.exe"
    )
    foreach ($loc in $locations) {
        if (Get-Command $loc -ErrorAction SilentlyContinue) {
            $psexec = $loc
            break
        }
    }
    if (-not $psexec) {
        Write-Host "[SKIP] PsExec not found." -ForegroundColor Yellow
        return $false
    }

    try {
        $dir = Split-Path $RemoteExePath -Parent
        $cmd = "taskkill /F /IM chipSetter.exe 2>nul && taskkill /F /IM gdbserver.exe 2>nul && cd /d `"$dir`" && start `"`" gdbserver.exe --once 0.0.0.0:$Port chipSetter.exe"
        & $psexec "\\$TargetIp" -s -d cmd /c $cmd
        Write-Host "[OK] gdbserver started via PsExec" -ForegroundColor Green
        return $true
    } catch {
        Write-Host "[SKIP] PsExec failed: $_" -ForegroundColor Yellow
        return $false
    }
}

# ---- Method 3: Manual instructions ----
function Show-ManualInstructions {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor DarkYellow
    Write-Host "  Cannot auto-start gdbserver on target" -ForegroundColor DarkYellow
    Write-Host "  Run manually on GNC (${TargetIp}):" -ForegroundColor DarkYellow
    Write-Host "========================================" -ForegroundColor DarkYellow
    Write-Host ""
    Write-Host "  cmd.exe:" -ForegroundColor White
    Write-Host "    cd C:\Users\googol\Desktop\share\chipSetter" -ForegroundColor Cyan
    Write-Host "    gdbserver.exe --once 0.0.0.0:${Port} chipSetter.exe" -ForegroundColor Cyan
    Write-Host ""
}

# ---- Main ----
$success = $false
if (-not $success) { $success = Start-ViaWinRM }
if (-not $success) { $success = Start-ViaPsExec }
if (-not $success) { Show-ManualInstructions }
