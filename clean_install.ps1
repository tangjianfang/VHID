$devcon = "D:\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe"
$stageDir = "D:\Github\VHID\build\driver\Release\package"

Write-Host "=== Removing old device instances ==="
& $devcon remove "Root\VHID" 2>&1
Write-Host "Remove exit: $LASTEXITCODE"

Write-Host "=== Listing driver store ==="
pnputil /enum-drivers 2>&1 | Select-String -Pattern "vhid" -Context 3,3

Write-Host "=== Copying vhid.sys to system32\drivers ==="
Copy-Item "$stageDir\vhid.sys" "$env:SystemRoot\System32\drivers\vhid.sys" -Force
if (Test-Path "$env:SystemRoot\System32\drivers\vhid.sys") {
    Write-Host "vhid.sys copied successfully"
    Get-Item "$env:SystemRoot\System32\drivers\vhid.sys" | Select-Object FullName, Length
} else {
    Write-Host "FAILED to copy vhid.sys"
}

Write-Host "=== Creating vhid service ==="
sc.exe delete vhid 2>$null
sc.exe create vhid type= kernel start= demand binPath= "$env:SystemRoot\System32\drivers\vhid.sys" displayname= "Virtual HID Driver" 2>&1
Write-Host "sc create exit: $LASTEXITCODE"

Write-Host "=== Service status ==="
sc.exe query vhid 2>&1

Write-Host "=== Installing fresh device ==="
& $devcon install "$stageDir\vhid.inf" "Root\VHID" 2>&1
Write-Host "devcon install exit: $LASTEXITCODE"

Write-Host "=== Final verification ==="
& $devcon status "Root\VHID" 2>&1
& $devcon driverfiles "Root\VHID" 2>&1

sc.exe query vhid 2>&1

$controlDevice = "\\.\VHidControl"
try {
    $handle = [System.IO.File]::Open($controlDevice, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
    Write-Host "SUCCESS: $controlDevice is accessible!"
    $handle.Close()
} catch {
    Write-Host "Cannot open ${controlDevice}: $($_.Exception.Message)"
}

Write-Host "=== Recent system events for vhid ==="
Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Service Control Manager'; Level=2,3; StartTime=(Get-Date).AddMinutes(-10)} -MaxEvents 10 -ErrorAction SilentlyContinue | Where-Object { $_.Message -match 'vhid' } | Format-List TimeCreated, Message
