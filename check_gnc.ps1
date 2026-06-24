$credFile = "$env:USERPROFILE\.chipsetter_cred.xml"
if (-not (Test-Path $credFile)) {
    Write-Host "No credential file found" -ForegroundColor Red
    exit 1
}
$cred = Import-Clixml -Path $credFile

Invoke-Command -ComputerName 192.168.1.2 -Credential $cred -ScriptBlock {
    $ErrorActionPreference = "Continue"

    taskkill /f /im chipSetter.exe 2>$null
    taskkill /f /im gdbserver.exe 2>$null
    Start-Sleep 1

    $dir = "C:\Users\googol\Desktop\share\chipSetter"
    $log = "$dir\chipsetter_debug.log"

    Write-Host "=== Debug log (last 30 lines) ==="
    if (Test-Path $log) {
        Get-Content $log -Tail 30
    } else {
        Write-Host "(no log file)"
    }

    Write-Host ""
    Write-Host "=== Files in chipSetter dir ==="
    Get-ChildItem $dir -File | Select Name, Length, LastWriteTime | Format-Table -AutoSize

    Write-Host ""
    Write-Host "=== gdbserver stderr ==="
    $stderrFile = "$env:TEMP\gdb_stderr.txt"
    if (Test-Path $stderrFile) {
        Get-Content $stderrFile
    } else {
        Write-Host "(no stderr file)"
    }
}
