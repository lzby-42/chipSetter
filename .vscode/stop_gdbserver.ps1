param(
    [string]$TargetIp = "192.168.1.2",
    [string]$RemoteUser = "192.168.1.2\googol"
)

$ErrorActionPreference = "Continue"

$credFile = "$env:USERPROFILE\.chipsetter_cred.xml"
$cred = $null
if (Test-Path $credFile) {
    try { $cred = Import-Clixml -Path $credFile } catch {}
}

if (-not $cred) {
    Write-Host "[ERROR] No saved credential. Run start_gdbserver.ps1 first." -ForegroundColor Red
    exit 1
}

try {
    $session = New-PSSession -ComputerName $TargetIp -Credential $cred -ErrorAction Stop
    Invoke-Command -Session $session -ScriptBlock {
        Get-Process -Name "chipSetter" -ErrorAction SilentlyContinue | Stop-Process -Force
        Get-Process -Name "gdbserver" -ErrorAction SilentlyContinue | Stop-Process -Force
        schtasks /delete /tn "chipSetter-gdbserver" /f 2>$null
        Write-Host "[OK] gdbserver & chipSetter killed on remote."
    }
    Remove-PSSession $session
    Write-Host "[OK] Done."
} catch {
    Write-Host "[ERROR] Failed: $_" -ForegroundColor Red
    exit 1
}
