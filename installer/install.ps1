<#
.SYNOPSIS
    VHID driver install script (test-signed).
    Must run as Administrator.
#>
[CmdletBinding()]
param(
    [switch] $AutoReboot
)

$ErrorActionPreference = 'Stop'

# ---- Check admin ----
$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$isAdmin = ([Security.Principal.WindowsPrincipal] $id).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] Please run this script as Administrator!" -ForegroundColor Red
    Write-Host "Right-click PowerShell -> 'Run as Administrator', then retry."
    exit 1
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

# ---- Verify files ----
$requiredFiles = @('vhid.sys', 'vhid.inf', 'vhid.cat', 'vhid-test.cer')
foreach ($f in $requiredFiles) {
    $path = Join-Path $ScriptDir $f
    if (-not (Test-Path $path)) {
        Write-Host "[ERROR] Missing file: $f" -ForegroundColor Red
        exit 1
    }
}
Write-Host "[OK] All driver files present." -ForegroundColor Green

# ---- Step 1: Install certificate ----
Write-Host ""
Write-Host "=== Step 1/3: Install test-signing certificate ===" -ForegroundColor Cyan
$cerPath = Join-Path $ScriptDir 'vhid-test.cer'

$existingCerts = Get-ChildItem Cert:\LocalMachine\TrustedPublisher | Where-Object { $_.Subject -match 'VHID' }
if ($existingCerts) {
    Write-Host "  Certificate already in TrustedPublisher, skipping."
} else {
    certutil -addstore 'Root' $cerPath | Out-Null
    certutil -addstore 'TrustedPublisher' $cerPath | Out-Null
    Write-Host "  Certificate imported to Root and TrustedPublisher." -ForegroundColor Green
}

# ---- Step 2: Enable test signing ----
Write-Host ""
Write-Host "=== Step 2/3: Enable test signing ===" -ForegroundColor Cyan

$bcdOutput = bcdedit /enum '{current}' 2>&1 | Out-String
if ($bcdOutput -match 'testsigning\s+Yes') {
    Write-Host "  Test signing already enabled, skipping."
    $needReboot = $false
} else {
    bcdedit /set testsigning on | Out-Null
    Write-Host "  Test signing enabled (reboot required)." -ForegroundColor Yellow
    $needReboot = $true
}

# ---- Step 3: Install driver ----
Write-Host ""
Write-Host "=== Step 3/3: Install driver ===" -ForegroundColor Cyan
$infPath = Join-Path $ScriptDir 'vhid.inf'

# Remove old instances if any
$oldDevices = Get-PnpDevice -InstanceId 'ROOT\VHID\*' -ErrorAction SilentlyContinue
if (-not $oldDevices) {
    $oldDevices = Get-PnpDevice -InstanceId 'ROOT\HIDCLASS\*' -ErrorAction SilentlyContinue |
                  Where-Object { $_.HardwareID -contains 'Root\VHID' }
}
if ($oldDevices) {
    Write-Host "  Removing old device instances..."
    foreach ($dev in $oldDevices) {
        pnputil /remove-device $dev.InstanceId 2>$null | Out-Null
    }
}

# Add driver package to store and install
Write-Host "  Adding driver package..."
$pnpResult = pnputil /add-driver $infPath /install 2>&1 | Out-String
Write-Host $pnpResult

if ($pnpResult -match 'successfully|Published Name') {
    Write-Host "  Driver installed successfully!" -ForegroundColor Green
} else {
    Write-Host "  Driver package added (may need reboot to take effect)." -ForegroundColor Yellow
    $needReboot = $true
}

# ---- Done ----
Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
if ($needReboot) {
    Write-Host "Installation complete! Reboot required for driver to load." -ForegroundColor Yellow
    Write-Host "After reboot, a 'Test Mode' watermark will appear on the desktop (this is normal)." -ForegroundColor DarkGray
    Write-Host ""
    if ($AutoReboot) {
        Write-Host "Rebooting in 5 seconds..." -ForegroundColor Yellow
        shutdown /r /t 5
    } else {
        $choice = Read-Host "Reboot now? (Y/N)"
        if ($choice -eq 'Y' -or $choice -eq 'y') {
            shutdown /r /t 0
        } else {
            Write-Host "Please reboot manually later."
        }
    }
} else {
    $dev = Get-PnpDevice -FriendlyName '*Virtual HID*' -ErrorAction SilentlyContinue |
           Where-Object { $_.Status -eq 'OK' } | Select-Object -First 1
    if ($dev) {
        Write-Host "Driver installed and running!" -ForegroundColor Green
        Write-Host "Device: $($dev.FriendlyName)" -ForegroundColor Green
    } else {
        Write-Host "Driver installed. If device not visible, please reboot." -ForegroundColor Yellow
    }
}
Write-Host "==========================================" -ForegroundColor Cyan
<#
.SYNOPSIS
    VHID 虚拟 HID 设备驱动安装脚本（测试签名）。
    必须以管理员身份运行。

.DESCRIPTION
    1. 导入测试签名证书到 Root + TrustedPublisher
    2. 启用测试签名模式（bcdedit）
    3. 安装驱动（pnputil）
    4. 提示是否立即重启

.EXAMPLE
    # 以管理员身份运行 PowerShell：
    .\install.ps1

.EXAMPLE
    # 静默安装（自动重启）：
    .\install.ps1 -AutoReboot
#>
[CmdletBinding()]
param(
    [switch] $AutoReboot
)

$ErrorActionPreference = 'Stop'

# ---- Check admin ----
$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$isAdmin = ([Security.Principal.WindowsPrincipal] $id).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] 请以管理员身份运行此脚本！" -ForegroundColor Red
    Write-Host "右键点击 PowerShell -> '以管理员身份运行'，然后重新执行。"
    exit 1
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

# ---- Verify files ----
$requiredFiles = @('vhid.sys', 'vhid.inf', 'vhid.cat', 'vhid-test.cer')
foreach ($f in $requiredFiles) {
    $path = Join-Path $ScriptDir $f
    if (-not (Test-Path $path)) {
        Write-Host "[ERROR] 缺少文件: $f" -ForegroundColor Red
        exit 1
    }
}
Write-Host "[OK] 所有驱动文件已就绪。" -ForegroundColor Green

# ---- Step 1: Install certificate ----
Write-Host ""
Write-Host "=== 步骤 1/3: 安装测试签名证书 ===" -ForegroundColor Cyan
$cerPath = Join-Path $ScriptDir 'vhid-test.cer'

$existingCerts = Get-ChildItem Cert:\LocalMachine\TrustedPublisher | Where-Object { $_.Subject -match 'VHID' }
if ($existingCerts) {
    Write-Host "  证书已存在于 TrustedPublisher，跳过。"
} else {
    certutil -addstore 'Root' $cerPath | Out-Null
    certutil -addstore 'TrustedPublisher' $cerPath | Out-Null
    Write-Host "  证书已导入到 Root 和 TrustedPublisher。" -ForegroundColor Green
}

# ---- Step 2: Enable test signing ----
Write-Host ""
Write-Host "=== 步骤 2/3: 启用测试签名 ===" -ForegroundColor Cyan

$bcdOutput = bcdedit /enum '{current}' 2>&1 | Out-String
if ($bcdOutput -match 'testsigning\s+Yes') {
    Write-Host "  测试签名已启用，跳过。"
    $needReboot = $false
} else {
    bcdedit /set testsigning on | Out-Null
    Write-Host "  测试签名已启用（需要重启生效）。" -ForegroundColor Yellow
    $needReboot = $true
}

# ---- Step 3: Install driver ----
Write-Host ""
Write-Host "=== 步骤 3/3: 安装驱动 ===" -ForegroundColor Cyan
$infPath = Join-Path $ScriptDir 'vhid.inf'

# Remove old instances if any
$oldDevices = Get-PnpDevice -InstanceId 'ROOT\VHID\*' -ErrorAction SilentlyContinue
if ($oldDevices) {
    Write-Host "  移除旧设备实例..."
    foreach ($dev in $oldDevices) {
        pnputil /remove-device $dev.InstanceId 2>$null | Out-Null
    }
}

# Add driver package to store
Write-Host "  添加驱动包到驱动商店..."
$pnpResult = pnputil /add-driver $infPath /install 2>&1 | Out-String
Write-Host $pnpResult

if ($pnpResult -match '已成功|successfully') {
    Write-Host "  驱动安装成功！" -ForegroundColor Green
} else {
    # pnputil may fail on first try before reboot for testsigning
    Write-Host "  驱动包已添加（可能需要重启后才能加载）。" -ForegroundColor Yellow
    $needReboot = $true
}

# ---- Done ----
Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
if ($needReboot) {
    Write-Host "安装完成！需要重启才能使驱动生效。" -ForegroundColor Yellow
    Write-Host "重启后，桌面右下角会出现 '测试模式' 水印，这是正常的。" -ForegroundColor DarkGray
    Write-Host ""
    if ($AutoReboot) {
        Write-Host "将在 5 秒后自动重启..." -ForegroundColor Yellow
        shutdown /r /t 5
    } else {
        $choice = Read-Host "是否立即重启？(Y/N)"
        if ($choice -eq 'Y' -or $choice -eq 'y') {
            shutdown /r /t 0
        } else {
            Write-Host "请稍后手动重启。"
        }
    }
} else {
    # Check if driver is actually running
    $dev = Get-PnpDevice -InstanceId 'ROOT\VHID\*' -ErrorAction SilentlyContinue |
           Where-Object { $_.Status -eq 'OK' }
    if ($dev) {
        Write-Host "驱动已安装并正在运行！" -ForegroundColor Green
        Write-Host "设备: $($dev.FriendlyName)" -ForegroundColor Green
    } else {
        Write-Host "驱动已安装。如设备未出现，请重启后再检查。" -ForegroundColor Yellow
    }
}
Write-Host "==========================================" -ForegroundColor Cyan
