$cred = Import-Clixml "$env:USERPROFILE\.chipsetter_cred.xml"
$s = New-PSSession 192.168.1.2 -Credential $cred
$lines = Invoke-Command $s {
    $dir = 'C:\Users\googol\Desktop\share\chipSetter\logs'
    $latest = Get-ChildItem $dir -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $all = Get-Content $latest.FullName
    $start = 0
    for ($i = 0; $i -lt $all.Count; $i++) {
        if ($all[$i] -match 'AXIS DIAGNOSTICS') { $start = $i; break }
    }
    # Print axes 9-16 (skip axes 1-8)
    $count = 0
    for ($i = $start; $i -lt $all.Count; $i++) {
        if ($all[$i] -match '^[0-9].*Axis [0-9]') { $count++ }
        if ($count -ge 9) { Write-Host $all[$i] }
        if ($count -ge 17) { break }
    }
}
Remove-PSSession $s
$lines | ForEach-Object { Write-Host $_ }
