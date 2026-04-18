$cert = Get-ChildItem Cert:\LocalMachine\My -CodeSigningCert -ErrorAction SilentlyContinue | Where-Object { $_.Subject -match 'VHID' }
if (-not $cert) {
    Write-Host "Cert not found in LocalMachine\My, checking CurrentUser\My..."
    $cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue | Where-Object { $_.Subject -match 'VHID' }
    $storeFlag = "" # Default is CurrentUser
} else {
    $storeFlag = "/sm"
}

if ($cert) {
    Write-Host "Found cert: $($cert.Thumbprint) in $($cert.PSParentPath)"
    $signtool = Get-ChildItem "D:\Windows Kits\10\bin" -Filter "signtool.exe" -Recurse | Where-Object { $_.FullName -match 'x64' } | Select-Object -First 1
    $sysFile = "D:\Github\VHID\build\driver\Release\vhid.sys"
    
    $cmdArgs = @("sign", "/v")
    if ($storeFlag) { $cmdArgs += $storeFlag }
    $cmdArgs += @("/sha1", $cert.Thumbprint, "/t", "http://timestamp.digicert.com", "/fd", "sha256", $sysFile)
    
    Write-Host "Running: $($signtool.FullName) $($cmdArgs -join ' ')"
    & $signtool.FullName $cmdArgs
    Write-Host "Sign exit: $LASTEXITCODE"
} else {
    Write-Host "VHID Certificate not found anywhere."
}
