param(
    [string]$SourceDir = "$PSScriptRoot\..\debug_deploy",
    [string]$TargetShare = "\\192.168.1.2\share\chipSetter",
    [string]$TargetIp = "192.168.1.2",
    [string]$RemoteUser = "googol"
)

$ErrorActionPreference = "Stop"

Write-Host "Deploying to $TargetShare ..." -ForegroundColor Cyan

# ---- Establish SMB connection ----
$credFile = "$env:USERPROFILE\.chipsetter_cred.xml"
if (Test-Path $credFile) {
    try {
        $cred = Import-Clixml -Path $credFile
        $password = $cred.GetNetworkCredential().Password
        net use "\\$TargetIp\share" /user:$RemoteUser $password /persistent:no 2>$null | Out-Null
        Write-Host "[OK] SMB connection established." -ForegroundColor Green
    } catch {
        Write-Host "[WARN] Could not load credential, trying without..." -ForegroundColor Yellow
    }
}

# ---- Run robocopy ----
robocopy $SourceDir $TargetShare *.* /R:3 /W:5 /NP

if ($LASTEXITCODE -ge 8) {
    Write-Host "[ERROR] robocopy failed with code: $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

Write-Host "[OK] Deploy complete (robocopy code: $LASTEXITCODE)" -ForegroundColor Green
exit 0
