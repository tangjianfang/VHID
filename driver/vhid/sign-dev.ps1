<#
.SYNOPSIS
    Dev-only signing helper for the VHID driver. Creates a self-signed code
    signing cert (once), generates the catalog, and signs vhid.sys + vhid.cat.

.DESCRIPTION
    For development machines only. The target machine must have test signing
    enabled (bcdedit /set testsigning on) and the cert installed in the
    Trusted Root + Trusted Publisher stores. Both happen automatically on
    first run when launched as Administrator.

    For release: do NOT use this script. Buy an EV cert and submit the package
    to Microsoft Partner Center for attestation signing instead.

.PARAMETER DriverDir
    Folder containing vhid.inf + the built vhid.sys. Defaults to this script's
    folder.

.PARAMETER PfxPath
    Path to the dev .pfx. Created on first run if missing.

.PARAMETER PfxPassword
    Password for the .pfx. Defaults to "devpass" — fine for a local dev cert.

.PARAMETER Arch
    Architecture passed to Inf2Cat (default: 10_x64,Server10_x64).

.EXAMPLE
    # First run on a fresh dev machine (run as Administrator):
    .\sign-dev.ps1

.EXAMPLE
    # Subsequent CI runs (cert already provisioned):
    .\sign-dev.ps1 -PfxPath C:\secrets\vhid-dev.pfx -PfxPassword $env:PFX_PWD
#>
[CmdletBinding()]
param(
    [string] $DriverDir   = $PSScriptRoot,
    [string] $PfxPath     = (Join-Path $PSScriptRoot 'vhid-dev.pfx'),
    [string] $PfxPassword = 'devpass',
    [string] $Arch        = '10_x64,Server10_x64',
    [string] $Subject     = 'CN=VHID Dev'
)

$ErrorActionPreference = 'Stop'

function Find-Tool {
    param([string] $Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # Fall back to common WDK / SDK locations.
    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "${env:ProgramFiles}\Windows Kits\10\bin"
    ) | Where-Object { Test-Path $_ }

    foreach ($root in $roots) {
        $hit = Get-ChildItem $root -Recurse -Filter $Name -ErrorAction SilentlyContinue |
               Where-Object { $_.FullName -match '\\x64\\' } |
               Sort-Object FullName -Descending |
               Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    throw "Could not locate $Name. Install the Windows SDK / WDK and ensure it's on PATH."
}

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal] $id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

# ---- Locate tools ----
$signtool = Find-Tool 'signtool.exe'
$inf2cat  = Find-Tool 'Inf2Cat.exe'
Write-Host "signtool: $signtool"
Write-Host "Inf2Cat : $inf2cat"

# ---- Provision cert if missing ----
if (-not (Test-Path $PfxPath)) {
    Write-Host "Creating self-signed dev cert at $PfxPath ..."
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $Subject `
        -KeyUsage DigitalSignature `
        -CertStoreLocation Cert:\CurrentUser\My `
        -KeyExportPolicy Exportable `
        -NotAfter (Get-Date).AddYears(5)

    $pwd = ConvertTo-SecureString $PfxPassword -AsPlainText -Force
    Export-PfxCertificate -Cert $cert -FilePath $PfxPath -Password $pwd | Out-Null

    $cerPath = [IO.Path]::ChangeExtension($PfxPath, '.cer')
    Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null

    if (Test-Admin) {
        Write-Host "Installing $cerPath into Root + TrustedPublisher ..."
        certutil -addstore 'Root'             $cerPath | Out-Null
        certutil -addstore 'TrustedPublisher' $cerPath | Out-Null
    } else {
        Write-Warning "Not running as admin: skipped Root/TrustedPublisher install."
        Write-Warning "Re-run elevated or import $cerPath manually before installing the driver."
    }
}

# ---- Sanity-check driver dir ----
$inf = Join-Path $DriverDir 'vhid.inf'
$sys = Join-Path $DriverDir 'vhid.sys'
if (-not (Test-Path $inf)) { throw "Missing $inf" }
if (-not (Test-Path $sys)) { throw "Missing $sys (build the driver first)" }

# ---- Generate catalog ----
Write-Host "Running Inf2Cat ..."
& $inf2cat /driver:$DriverDir /os:$Arch
if ($LASTEXITCODE -ne 0) { throw "Inf2Cat failed with exit code $LASTEXITCODE" }

$cat = Join-Path $DriverDir 'vhid.cat'
if (-not (Test-Path $cat)) { throw "Inf2Cat did not produce vhid.cat" }

# ---- Sign sys + cat ----
Write-Host "Signing $sys and $cat ..."
& $signtool sign `
    /fd SHA256 `
    /f $PfxPath `
    /p $PfxPassword `
    /tr http://timestamp.digicert.com /td SHA256 `
    $sys $cat
if ($LASTEXITCODE -ne 0) { throw "signtool failed with exit code $LASTEXITCODE" }

Write-Host ''
Write-Host 'Done. To install:' -ForegroundColor Green
Write-Host '  1) (once)  bcdedit /set testsigning on  ; shutdown /r /t 0'
Write-Host "  2)         devcon install `"$inf`" Root\VHID"
