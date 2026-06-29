$cred = Import-Clixml "$env:USERPROFILE\.chipsetter_cred.xml"
$s = New-PSSession 192.168.1.2 -Credential $cred
$lines = Invoke-Command $s {
    $log = 'C:\Users\googol\Desktop\share\chipSetter\chipsetter_debug.log'
    if (Test-Path $log) {
        $all = Get-Content $log
        Write-Host "=== Log modified:" ((Get-Item $log).LastWriteTime)
        Write-Host "=== Lines:" $all.Count
        Write-Host ""
        Write-Host "=== Errors and warnings (last 30) ==="
        $all | Select-String -Pattern 'error|warn|fail|parse|config|ERROR|WARN|FAIL|PARSE|CONFIG|0x' -CaseSensitive | Select-Object -Last 30 | ForEach-Object { Write-Host $_ }
        Write-Host ""
        Write-Host "=== Last 25 lines ==="
        $all | Select-Object -Last 25 | ForEach-Object { Write-Host $_ }
    } else { Write-Host "(no log)" }
}
Remove-PSSession $s
$lines | ForEach-Object { Write-Host $_ }
