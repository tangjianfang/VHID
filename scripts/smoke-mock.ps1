$ErrorActionPreference = 'Stop'
$cli = Resolve-Path .\build\src\cli\Debug\vhid-cli.exe

$tmp = New-Item -ItemType Directory -Force -Path .\.smoke | Select-Object -ExpandProperty FullName
$devOut  = Join-Path $tmp 'dev.out.txt'
$devErr  = Join-Path $tmp 'dev.err.txt'
$devIn   = Join-Path $tmp 'dev.in.txt'
$hostOut = Join-Path $tmp 'host.out.txt'
$hostErr = Join-Path $tmp 'host.err.txt'
$hostIn  = Join-Path $tmp 'host.in.txt'

# Pre-create input scripts:
#   device side: submit one input report, then keep the pipe alive while the
#                host runs its commands; quit last so nothing is dropped.
#   host  side: send one output, do a synchronous getf round-trip, then quit.
@"
in 11 22 33
sleep 1500
quit
"@ | Out-File -Encoding ascii $devIn

@"
sleep 200
out aa bb cc
getf
quit
"@ | Out-File -Encoding ascii $hostIn

# 1) Start device first; it blocks on ConnectNamedPipe until a host connects.
$dev = Start-Process -FilePath $cli `
    -ArgumentList 'mock-device' `
    -RedirectStandardInput  $devIn `
    -RedirectStandardOutput $devOut `
    -RedirectStandardError  $devErr `
    -PassThru -WindowStyle Hidden

Start-Sleep -Milliseconds 500

# 2) Start host; should connect to the pipe.
$hst = Start-Process -FilePath $cli `
    -ArgumentList 'mock-host' `
    -RedirectStandardInput  $hostIn `
    -RedirectStandardOutput $hostOut `
    -RedirectStandardError  $hostErr `
    -PassThru -WindowStyle Hidden

# Give them up to 5s to finish their script.
$dev.WaitForExit(5000) | Out-Null
$hst.WaitForExit(5000) | Out-Null
if (-not $dev.HasExited) { $dev.Kill() }
if (-not $hst.HasExited) { $hst.Kill() }

Write-Host '--- device stdout ---'
Get-Content $devOut -ErrorAction SilentlyContinue
Write-Host '--- device stderr ---'
Get-Content $devErr -ErrorAction SilentlyContinue
Write-Host '--- host   stdout ---'
Get-Content $hostOut -ErrorAction SilentlyContinue
Write-Host '--- host   stderr ---'
Get-Content $hostErr -ErrorAction SilentlyContinue

$ok = $true
$devText  = (Get-Content $devOut  -Raw -ErrorAction SilentlyContinue)
$hostText = (Get-Content $hostOut -Raw -ErrorAction SilentlyContinue)

# Expectation: the host should see [INPUT ] containing 11 22 33,
# the device should see [OUTPUT] containing aa bb cc and a [FEAT-GET] line.
if ($hostText -notmatch '\[INPUT[^\]]*\][^\n]*11 22 33') { Write-Host 'FAIL: host did not receive input report'; $ok = $false }
if ($devText  -notmatch '\[OUTPUT[^\]]*\][^\n]*aa bb cc') { Write-Host 'FAIL: device did not receive output report'; $ok = $false }
if ($devText  -notmatch '\[FEAT-GET\]')                   { Write-Host 'FAIL: device did not see feature-get'; $ok = $false }

if ($ok) { Write-Host 'SMOKE TEST PASSED'; exit 0 } else { Write-Host 'SMOKE TEST FAILED'; exit 1 }
