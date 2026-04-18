$devcon = "D:\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe"
$stageDir = "D:\Github\VHID\build\driver\Release\package"

Write-Host "--- Cleanup ---"
sc.exe stop vhid 2>$null
sc.exe delete vhid 2>$null
& $devcon remove "Root\VHID"
& $devcon remove "ROOT\HIDCLASS*"

Write-Host "--- Prep Files ---"
Remove-Item "$env:SystemRoot\System32\drivers\vhid.sys" -Force -ErrorAction SilentlyContinue
Copy-Item "$stageDir\vhid.sys" "$env:SystemRoot\System32\drivers\vhid.sys" -Force
Get-Item "$env:SystemRoot\System32\drivers\vhid.sys" | Select-Object FullName, Length

Write-Host "--- Service Start Test ---"
sc.exe create vhid type= kernel start= demand binPath= "$env:SystemRoot\System32\drivers\vhid.sys" displayname= "Virtual HID Driver"
sc.exe start vhid 2>&1

Write-Host "--- Devcon Install ---"
& $devcon install "$stageDir\vhid.inf" "Root\VHID" 2>&1

Write-Host "--- Final Check ---"
sc.exe query vhid
& $devcon status "Root\VHID"
& $devcon status "ROOT\HIDCLASS*"

Write-Host "--- Event Log ---"
Get-WinEvent -FilterHashtable @{LogName='System'; StartTime=(Get-Date).AddMinutes(-5)} -ErrorAction SilentlyContinue | Where-Object { $_.Message -match 'vhid' -or $_.ProviderName -match 'vhid' } | Select-Object TimeCreated, Message | Format-List
