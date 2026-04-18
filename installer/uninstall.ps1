<#
.SYNOPSIS
    VHID driver uninstall script.
    Must run as Administrator.
#>
[CmdletBinding()]
param(
    [switch] $CleanAll
)

$ErrorActionPreference = 'Stop'

# ---- Check admin ----
$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$isAdmin = ([Security.Principal.WindowsPrincipal] $id).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] Please run this script as Administrator!" -ForegroundColor Red
    exit 1
}

# ---- Step 1: Remove device instances ----
Write-Host "=== Step 1/3: Remove devices ===" -ForegroundColor Cyan
$removed = $false

$devices = Get-PnpDevice -InstanceId 'ROOT\VHID\*' -ErrorAction SilentlyContinue
foreach ($dev in $devices) {
    Write-Host "  Removing: $($dev.InstanceId)"
    pnputil /remove-device $dev.InstanceId 2>$null | Out-Null
    $removed = $true
}

$hidDevices = Get-PnpDevice -InstanceId 'ROOT\HIDCLASS\*' -ErrorAction SilentlyContinue |
              Where-Object { $_.HardwareID -contains 'Root\VHID' }
foreach ($dev in $hidDevices) {
    Write-Host "  Removing: $($dev.InstanceId)"
    pnputil /remove-device $dev.InstanceId 2>$null | Out-Null
    $removed = $true
}

if ($removed) {
    Write-Host "  Devices removed." -ForegroundColor Green
} else {
    Write-Host "  No VHID devices found."
}

# ---- Step 2: Remove driver package from store ----
Write-Host ""
Write-Host "=== Step 2/3: Remove driver package ===" -ForegroundColor Cyan

$oemInfs = pnputil /enum-drivers 2>&1 | Out-String
$lines = $oemInfs -split "`n"
$candidateOem = $null
$oemNum = $null
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '(oem\d+\.inf)') {
        $candidateOem = $matches[1]
    }
    if ($lines[$i] -match 'vhid' -or $lines[$i] -match 'Virtual HID') {
        $oemNum = $candidateOem
        break
    }
}

if ($oemNum) {
    Write-Host "  Deleting driver package: $oemNum"
    pnputil /delete-driver $oemNum /uninstall /force 2>$null | Out-Null
    Write-Host "  Driver package deleted." -ForegroundColor Green
} else {
    Write-Host "  No VHID driver package found in store."
}

# ---- Step 3: Remove service and driver file ----
Write-Host ""
Write-Host "=== Step 3/3: Cleanup service and files ===" -ForegroundColor Cyan

$svc = Get-Service -Name 'vhid' -ErrorAction SilentlyContinue
if ($svc) {
    sc.exe stop vhid 2>$null | Out-Null
    sc.exe delete vhid 2>$null | Out-Null
    Write-Host "  Service 'vhid' deleted." -ForegroundColor Green
} else {
    Write-Host "  Service 'vhid' not found."
}

$sysPath = "$env:SystemRoot\System32\drivers\vhid.sys"
if (Test-Path $sysPath) {
    Remove-Item $sysPath -Force -ErrorAction SilentlyContinue
    Write-Host "  $sysPath deleted." -ForegroundColor Green
} else {
    Write-Host "  Driver file not present."
}

# ---- Optional: Clean certificates and testsigning ----
if ($CleanAll) {
    Write-Host ""
    Write-Host "=== Extra cleanup: certificates & test signing ===" -ForegroundColor Cyan

    $stores = @('Root', 'TrustedPublisher', 'My')
    foreach ($store in $stores) {
        $certs = Get-ChildItem "Cert:\LocalMachine\$store" | Where-Object { $_.Subject -match 'VHID' }
        foreach ($c in $certs) {
            Remove-Item $c.PSPath -Force
            Write-Host "  Removed cert from ${store}: $($c.Subject)"
        }
    }

    bcdedit /set testsigning off | Out-Null
    Write-Host "  Test signing disabled (reboot required)." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Uninstall complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
<#
.SYNOPSIS
    VHID 虚拟 HID 设备驱动卸载脚本。
    必须以管理员身份运行。

.DESCRIPTION
    1. 移除设备实例
    2. 从驱动商店删除驱动包
    3. 删除驱动文件
    4. 可选：移除测试签名证书和关闭测试签名

.EXAMPLE
    .\uninstall.ps1

.EXAMPLE
    # 同时清理证书和关闭测试签名：
    .\uninstall.ps1 -CleanAll
#>
[CmdletBinding()]
param(
    [switch] $CleanAll
)

$ErrorActionPreference = 'Stop'

# ---- Check admin ----
$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$isAdmin = ([Security.Principal.WindowsPrincipal] $id).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] 请以管理员身份运行此脚本！" -ForegroundColor Red
    exit 1
}

# ---- Step 1: Remove device instances ----
Write-Host "=== 步骤 1/3: 移除设备 ===" -ForegroundColor Cyan
$devices = Get-PnpDevice -InstanceId 'ROOT\VHID\*' -ErrorAction SilentlyContinue
if ($devices) {
    foreach ($dev in $devices) {
        Write-Host "  移除: $($dev.InstanceId)"
        pnputil /remove-device $dev.InstanceId 2>$null | Out-Null
    }
    # Also remove from HIDClass if devcon created them there
    $hidDevices = Get-PnpDevice -InstanceId 'ROOT\HIDCLASS\*' -ErrorAction SilentlyContinue |
                  Where-Object { $_.HardwareID -contains 'Root\VHID' }
    foreach ($dev in $hidDevices) {
        Write-Host "  移除: $($dev.InstanceId)"
        pnputil /remove-device $dev.InstanceId 2>$null | Out-Null
    }
    Write-Host "  设备已移除。" -ForegroundColor Green
} else {
    Write-Host "  未找到 VHID 设备。"
}

# ---- Step 2: Remove driver package from store ----
Write-Host ""
Write-Host "=== 步骤 2/3: 删除驱动包 ===" -ForegroundColor Cyan

# Find OEM inf number for our driver
$oemInfs = pnputil /enum-drivers 2>&1 | Out-String
$lines = $oemInfs -split "`n"
$oemNum = $null
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '(oem\d+\.inf)') {
        $candidateOem = $matches[1]
    }
    if ($lines[$i] -match 'vhid' -or $lines[$i] -match 'Virtual HID') {
        $oemNum = $candidateOem
        break
    }
}

if ($oemNum) {
    Write-Host "  删除驱动包: $oemNum"
    pnputil /delete-driver $oemNum /uninstall /force 2>$null | Out-Null
    Write-Host "  驱动包已删除。" -ForegroundColor Green
} else {
    Write-Host "  未在驱动商店中找到 VHID 驱动包。"
}

# ---- Step 3: Remove service and driver file ----
Write-Host ""
Write-Host "=== 步骤 3/3: 清理服务和文件 ===" -ForegroundColor Cyan

$svc = Get-Service -Name 'vhid' -ErrorAction SilentlyContinue
if ($svc) {
    sc.exe stop vhid 2>$null | Out-Null
    sc.exe delete vhid 2>$null | Out-Null
    Write-Host "  vhid 服务已删除。" -ForegroundColor Green
} else {
    Write-Host "  vhid 服务不存在。"
}

$sysPath = "$env:SystemRoot\System32\drivers\vhid.sys"
if (Test-Path $sysPath) {
    Remove-Item $sysPath -Force -ErrorAction SilentlyContinue
    Write-Host "  $sysPath 已删除。" -ForegroundColor Green
} else {
    Write-Host "  驱动文件不存在。"
}

# ---- Optional: Clean certificates and testsigning ----
if ($CleanAll) {
    Write-Host ""
    Write-Host "=== 额外清理: 证书和测试签名 ===" -ForegroundColor Cyan

    # Remove certs
    $stores = @('Root', 'TrustedPublisher', 'My')
    foreach ($store in $stores) {
        $certs = Get-ChildItem "Cert:\LocalMachine\$store" | Where-Object { $_.Subject -match 'VHID' }
        foreach ($c in $certs) {
            Remove-Item $c.PSPath -Force
            Write-Host "  已从 $store 移除证书: $($c.Subject)"
        }
    }

    # Disable test signing
    bcdedit /set testsigning off | Out-Null
    Write-Host "  测试签名已关闭（需重启生效）。" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "卸载完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
