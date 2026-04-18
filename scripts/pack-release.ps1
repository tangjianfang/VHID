# pack-release.ps1
# Bundle all runtime binaries, SDK headers, sample profiles, scripts and docs
# into a single distributable zip.
#
# Usage:   .\scripts\pack-release.ps1 [-Version 0.1.0] [-Config Release]

[CmdletBinding()]
param(
    [string]$Version = "0.1.0",
    [string]$Config  = "Release"
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$pkgName = "VHID-$Version-win-x64"
$dist    = Join-Path $root "dist"
$stage   = Join-Path $dist $pkgName
$zip     = Join-Path $dist "$pkgName.zip"

Write-Host "[pack] Cleaning $stage" -ForegroundColor Cyan
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zip)   { Remove-Item $zip   -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

# Layout
$dirs = @('bin','sdk\include\vhid','sdk\lib','samples\consumer_app','samples\profiles','scripts','docs','installer','third_party\nlohmann')
foreach ($d in $dirs) {
    New-Item -ItemType Directory -Force -Path (Join-Path $stage $d) | Out-Null
}

function Copy-IfExists($src, $dst) {
    if (Test-Path $src) {
        Copy-Item $src $dst -Force -Recurse
        Write-Host "  + $src" -ForegroundColor DarkGray
    } else {
        Write-Warning "missing: $src"
    }
}

Write-Host "[pack] Copying binaries" -ForegroundColor Cyan
Copy-IfExists "build\src\cli\$Config\vhid-cli.exe"                (Join-Path $stage 'bin')
Copy-IfExists "build\src\gui\$Config\vhid-gui.exe"                (Join-Path $stage 'bin')
Copy-IfExists "build\samples\consumer_app\$Config\consumer_app.exe" (Join-Path $stage 'samples\consumer_app')

Write-Host "[pack] Copying SDK" -ForegroundColor Cyan
Copy-IfExists "src\sdk\include\vhid\vhid_device.hpp"   (Join-Path $stage 'sdk\include\vhid')
Copy-IfExists "build\src\sdk\$Config\vhid_sdk.lib"     (Join-Path $stage 'sdk\lib')
Copy-IfExists "build\src\core\$Config\vhid_core.lib"   (Join-Path $stage 'sdk\lib')
Copy-IfExists "build\src\mock\$Config\vhid_mock.lib"   (Join-Path $stage 'sdk\lib')
Copy-IfExists "build\src\capture\$Config\vhid_capture.lib" (Join-Path $stage 'sdk\lib')
Copy-IfExists "third_party\nlohmann\json.hpp"          (Join-Path $stage 'third_party\nlohmann')
Copy-IfExists "third_party\nlohmann\LICENSE.MIT"       (Join-Path $stage 'third_party\nlohmann')

Write-Host "[pack] Copying scripts & docs" -ForegroundColor Cyan
Copy-IfExists "scripts\smoke-mock.ps1"    (Join-Path $stage 'scripts')
Copy-IfExists "scripts\gui-screenshot.ps1" (Join-Path $stage 'scripts')
Copy-IfExists "installer\install.ps1"     (Join-Path $stage 'installer')
Copy-IfExists "installer\uninstall.ps1"   (Join-Path $stage 'installer')
Copy-IfExists "installer\pack.ps1"        (Join-Path $stage 'installer')
Copy-IfExists "README.md"                 (Join-Path $stage 'docs')
Copy-IfExists "samples\consumer_app\src"  (Join-Path $stage 'samples\consumer_app')

# Generate a sample profile by capturing first device, if available.
Write-Host "[pack] Trying to capture a sample device profile" -ForegroundColor Cyan
$cli = Join-Path $stage 'bin\vhid-cli.exe'
if (Test-Path $cli) {
    try {
        $out = & $cli capture 0 -o (Join-Path $stage 'samples\profiles\sample-device-0.json') 2>&1
        Write-Host ($out -join "`n") -ForegroundColor DarkGray
    } catch {
        Write-Warning "capture failed: $_"
    }
}

# Top-level docs
$readme = @"
VHID $Version  -  Virtual HID Toolkit (Windows x64)
====================================================

Contents
--------
  bin/                  Runnable executables
    vhid-gui.exe        Razer-styled Win32 control center (recommended)
    vhid-cli.exe        Command line: capture / inject / mock
  sdk/
    include/vhid/       Public C++ headers
    lib/                Static libraries (link with /MD, MSVC v143)
  samples/
    consumer_app/       Source + prebuilt sample HID consumer
    profiles/           Captured device profiles (JSON)
  scripts/              Helper PowerShell scripts
  installer/            Driver-package install / uninstall scripts
  third_party/nlohmann/ Vendored JSON library (MIT, see LICENSE.MIT)
  docs/README.md        Full project documentation

Quick start
-----------
  1) Double-click bin\vhid-gui.exe                (GUI)
  2) Or:  bin\vhid-cli.exe capture list           (CLI)
          bin\vhid-cli.exe capture 0 -o my.json
          bin\vhid-cli.exe mock --profile my.json

Requirements
------------
  Windows 10/11 x64, Visual C++ Runtime 2015-2022 (typically preinstalled).

Build configuration
-------------------
  Built with: MSVC v143, /W4 /permissive- /Zc:__cplusplus /utf-8
  CMake config: $Config
  Package date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
  Source: https://github.com/tangjianfang/VHID
"@
Set-Content -Path (Join-Path $stage 'README.txt') -Value $readme -Encoding UTF8

# Manifest
Write-Host "[pack] Writing manifest.json" -ForegroundColor Cyan
$files = Get-ChildItem $stage -Recurse -File
$manifest = [ordered]@{
    name      = "VHID"
    version   = $Version
    config    = $Config
    platform  = "win-x64"
    built_at  = (Get-Date).ToString('o')
    files     = @($files | ForEach-Object {
        [ordered]@{
            path   = $_.FullName.Substring($stage.Length + 1).Replace('\','/')
            size   = $_.Length
            sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
        }
    })
}
$manifest | ConvertTo-Json -Depth 5 |
    Set-Content -Path (Join-Path $stage 'manifest.json') -Encoding UTF8

Write-Host "[pack] Compressing" -ForegroundColor Cyan
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal -Force

$zipInfo = Get-Item $zip
$sha = (Get-FileHash $zip -Algorithm SHA256).Hash
Write-Host ""
Write-Host "DONE  $zip" -ForegroundColor Green
Write-Host ("Size  {0:N1} KB" -f ($zipInfo.Length / 1KB))
Write-Host ("SHA   $sha")
