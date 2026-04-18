# Step 2: Extract thumbprint or cert info
$existingCert = Get-ChildItem Cert:\LocalMachine\My -CodeSigningCert -ErrorAction SilentlyContinue | Where-Object { $_.Subject -match 'VHID' }
if ($existingCert) {
    Write-Host "Existing VHID cert found: $($existingCert.Thumbprint)"
    $cert = $existingCert
} else {
    Write-Host "Creating self-signed code signing certificate..."
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=VHID Test Signing" -CertStoreLocation Cert:\LocalMachine\My -NotAfter (Get-Date).AddYears(5)
    Write-Host "Created cert: $($cert.Thumbprint)"
    
    $storePaths = @("Root", "TrustedPublisher")
    foreach ($path in $storePaths) {
        $store = New-Object System.Security.Cryptography.X509Certificates.X509Store $path, "LocalMachine"
        $store.Open("ReadWrite")
        $store.Add($cert)
        $store.Close()
        Write-Host "Added to $path"
    }
}

# Step 5: Find signtool
$signtool = Get-ChildItem "D:\Windows Kits\10\bin" -Filter "signtool.exe" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match 'x64' } | Select-Object -First 1
Write-Host "Signtool: $($signtool.FullName)"

# Step 6: Sign vhid.sys using SHA1 thumbprint for precision
$sysFile = "D:\Github\VHID\build\driver\Release\vhid.sys"
if ($signtool -and (Test-Path $sysFile)) {
    Write-Host "Signing vhid.sys with thumbprint $($cert.Thumbprint)..."
    & $signtool.FullName sign /v /sm /sha1 $($cert.Thumbprint) /t http://timestamp.digicert.com /fd sha256 $sysFile
    Write-Host "Sign exit: $LASTEXITCODE"
} else {
    Write-Host "Cannot sign: signtool=$($signtool) sysFile=$(Test-Path $sysFile)"
}
