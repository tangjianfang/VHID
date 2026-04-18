# Step 1: Enable test signing (needs admin)
Start-Process cmd -ArgumentList '/c bcdedit /set testsigning on' -Verb RunAs -Wait
Write-Host "Testsigning enabled (requires reboot to take effect)"

# Step 2: Check if we already have a test cert
$existingCert = Get-ChildItem Cert:\LocalMachine\My -CodeSigningCert -ErrorAction SilentlyContinue | Where-Object { $_.Subject -match 'VHID' }
if ($existingCert) {
    Write-Host "Existing VHID cert found: $($existingCert.Thumbprint)"
    $cert = $existingCert
} else {
    # Step 3: Create self-signed cert
    Write-Host "Creating self-signed code signing certificate..."
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=VHID Test Signing" -CertStoreLocation Cert:\LocalMachine\My -NotAfter (Get-Date).AddYears(5)
    Write-Host "Created cert: $($cert.Thumbprint)"
    
    # Step 4: Also add to Trusted Root (for test signing)
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store "Root","LocalMachine"
    $store.Open("ReadWrite")
    $store.Add($cert)
    $store.Close()
    Write-Host "Added to Trusted Root"

    # Also add to TrustedPublisher  
    $store2 = New-Object System.Security.Cryptography.X509Certificates.X509Store "TrustedPublisher","LocalMachine"
    $store2.Open("ReadWrite")
    $store2.Add($cert)
    $store2.Close()
    Write-Host "Added to TrustedPublisher"
}

# Step 5: Find signtool
$signtool = Get-ChildItem "D:\Windows Kits\10\bin\10.0.26100.0\x64" -Filter "signtool.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $signtool) {
    $signtool = Get-ChildItem "D:\Windows Kits\10\bin" -Filter "signtool.exe" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match 'x64' } | Select-Object -First 1
}
Write-Host "Signtool: $($signtool.FullName)"

# Step 6: Sign vhid.sys
$sysFile = "D:\Github\VHID\build\driver\Release\vhid.sys"
if ($signtool -and (Test-Path $sysFile)) {
    Write-Host "Signing vhid.sys..."
    & $signtool.FullName sign /v /s My /n "VHID Test Signing" /t http://timestamp.digicert.com /fd sha256 $sysFile
    Write-Host "Sign exit: $LASTEXITCODE"
} else {
    Write-Host "Cannot sign: signtool=$($signtool) sysFile=$(Test-Path $sysFile)"
}
