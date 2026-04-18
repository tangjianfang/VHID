<#
.SYNOPSIS
    Package VHID driver installer into a distributable ZIP.
    Run on the dev machine after building and signing the driver.

.EXAMPLE
    .\pack.ps1
    # Output: build\vhid-installer.zip
#>
$ErrorActionPreference = 'Stop'

$RepoRoot  = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$PkgDir    = Join-Path $RepoRoot 'build\driver\Release\package'
$InstDir   = Join-Path $RepoRoot 'installer'
$StageDir  = Join-Path $RepoRoot 'build\vhid-installer'
$ZipPath   = Join-Path $RepoRoot 'build\vhid-installer.zip'

# Clean staging
if (Test-Path $StageDir) { Remove-Item $StageDir -Recurse -Force }
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

# Copy driver files
foreach ($f in @('vhid.sys', 'vhid.inf', 'vhid.cat', 'vhid-test.cer')) {
    $src = Join-Path $PkgDir $f
    if (-not (Test-Path $src)) {
        Write-Error "Missing: $src (build and sign the driver first)"
    }
    Copy-Item $src $StageDir
}

# Copy scripts
Copy-Item (Join-Path $InstDir 'install.ps1')   $StageDir
Copy-Item (Join-Path $InstDir 'uninstall.ps1') $StageDir

# Generate README
$readme = @'
============================================================
  VHID - Virtual HID Device Driver Installer
============================================================

This package contains the VHID virtual HID device kernel
driver and installation tools. The driver is test-signed
and intended for development/testing environments only.

Files:
  vhid.sys        - Kernel driver
  vhid.inf        - Driver installation info
  vhid.cat        - Driver catalog (signed)
  vhid-test.cer   - Test-signing certificate
  install.ps1     - Install script
  uninstall.ps1   - Uninstall script

------------------------------------------------------------
  Installation
------------------------------------------------------------

1. Extract this ZIP to any folder

2. Open PowerShell as Administrator, cd to the folder

3. If execution policy blocks scripts, run:
     Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process

4. Run the installer:
     .\install.ps1

5. Reboot when prompted (first install only, to enable
   test signing mode)

6. After reboot, verify in Device Manager:
     Human Interface Devices > Virtual HID Device (VHID)

Silent install (auto-reboot):
     .\install.ps1 -AutoReboot

------------------------------------------------------------
  Uninstall
------------------------------------------------------------

Run as Administrator:
     .\uninstall.ps1

Full cleanup (remove certs + disable test signing):
     .\uninstall.ps1 -CleanAll

------------------------------------------------------------
  Verify
------------------------------------------------------------

After reboot, run in PowerShell:
  Get-PnpDevice -FriendlyName "*Virtual HID*"

Status should show OK.

------------------------------------------------------------
  Notes
------------------------------------------------------------

- Test-signed driver: a "Test Mode" watermark will appear
  on the desktop after enabling test signing. This is normal.

- Supported: Windows 10/11 x64, Windows Server 2016+.

- Do NOT use test-signed drivers in production.

============================================================
'@
$readme | Out-File -Encoding ASCII (Join-Path $StageDir 'README.txt')

# Create ZIP
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path "$StageDir\*" -DestinationPath $ZipPath -CompressionLevel Optimal

$size = [math]::Round((Get-Item $ZipPath).Length / 1024, 1)
Write-Host ""
Write-Host "Package created!" -ForegroundColor Green
Write-Host "Output: $ZipPath ($size KB)" -ForegroundColor Green
Write-Host ""
Write-Host "Usage:" -ForegroundColor Cyan
Write-Host "  1. Copy $ZipPath to target machine"
Write-Host "  2. Extract, run install.ps1 as Administrator"
Write-Host "  3. Reboot and verify"
<#
.SYNOPSIS
    鎵撳寘 VHID 椹卞姩瀹夎鍖呬负涓€涓彲鍒嗗彂鐨?ZIP 鏂囦欢銆?
    鍦ㄥ紑鍙戞満鍣ㄤ笂杩愯锛岀敓鎴愬彲鐩存帴甯﹀埌娴嬭瘯鏈哄畨瑁呯殑鍖呫€?

.DESCRIPTION
    鏀堕泦浠ヤ笅鏂囦欢鎵撳寘涓?vhid-installer.zip锛?
    - vhid.sys       (宸茬鍚嶇殑椹卞姩)
    - vhid.inf       (瀹夎淇℃伅)
    - vhid.cat       (宸茬鍚嶇殑鐩綍)
    - vhid-test.cer  (娴嬭瘯绛惧悕璇佷功)
    - install.ps1    (瀹夎鑴氭湰)
    - uninstall.ps1  (鍗歌浇鑴氭湰)
    - README.txt     (瀹夎璇存槑)

.EXAMPLE
    .\pack.ps1
    # 杈撳嚭: build\vhid-installer.zip
#>
$ErrorActionPreference = 'Stop'

$RepoRoot  = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$PkgDir    = Join-Path $RepoRoot 'build\driver\Release\package'
$InstDir   = Join-Path $RepoRoot 'installer'
$StageDir  = Join-Path $RepoRoot 'build\vhid-installer'
$ZipPath   = Join-Path $RepoRoot 'build\vhid-installer.zip'

# Clean staging
if (Test-Path $StageDir) { Remove-Item $StageDir -Recurse -Force }
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

# Copy driver files
foreach ($f in @('vhid.sys', 'vhid.inf', 'vhid.cat', 'vhid-test.cer')) {
    $src = Join-Path $PkgDir $f
    if (-not (Test-Path $src)) {
        Write-Error "缂哄皯鏂囦欢: $src (璇峰厛鏋勫缓骞剁鍚嶉┍鍔?"
    }
    Copy-Item $src $StageDir
}

# Copy scripts
Copy-Item (Join-Path $InstDir 'install.ps1')   $StageDir
Copy-Item (Join-Path $InstDir 'uninstall.ps1') $StageDir

# Generate README
@"
============================================================
  VHID - 铏氭嫙 HID 璁惧椹卞姩瀹夎鍖?
============================================================

鏈寘鍖呭惈 VHID 铏氭嫙 HID 璁惧鐨勫唴鏍搁┍鍔ㄥ強瀹夎宸ュ叿銆?
椹卞姩浣跨敤娴嬭瘯绛惧悕锛屼粎鐢ㄤ簬寮€鍙戝拰娴嬭瘯鐜銆?

鏂囦欢娓呭崟:
  vhid.sys        - 鍐呮牳椹卞姩鏂囦欢
  vhid.inf        - 椹卞姩瀹夎淇℃伅
  vhid.cat        - 椹卞姩绛惧悕鐩綍
  vhid-test.cer   - 娴嬭瘯绛惧悕璇佷功
  install.ps1     - 瀹夎鑴氭湰
  uninstall.ps1   - 鍗歌浇鑴氭湰

------------------------------------------------------------
  瀹夎姝ラ
------------------------------------------------------------

1. 瑙ｅ帇鏈?ZIP 鍒颁换鎰忕洰褰?

2. 浠ョ鐞嗗憳韬唤鎵撳紑 PowerShell锛岃繘鍏ヨВ鍘嬬洰褰?

3. 濡傛灉 PowerShell 鎵ц绛栫暐闄愬埗鑴氭湰杩愯锛屽厛鎵ц锛?
     Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process

4. 杩愯瀹夎鑴氭湰锛?
     .\install.ps1

5. 鎸夋彁绀洪噸鍚數鑴戯紙棣栨瀹夎闇€瑕侀噸鍚竴娆′互鍚敤娴嬭瘯绛惧悕锛?

6. 閲嶅惎鍚庯紝鍦ㄨ澶囩鐞嗗櫒涓彲浠ョ湅鍒帮細
     浜轰綋瀛﹁緭鍏ヨ澶?> Virtual HID Device (VHID)

闈欓粯瀹夎锛堣嚜鍔ㄩ噸鍚級锛?
     .\install.ps1 -AutoReboot

------------------------------------------------------------
  鍗歌浇姝ラ
------------------------------------------------------------

1. 浠ョ鐞嗗憳韬唤杩愯锛?
     .\uninstall.ps1

2. 瀹屽叏娓呯悊锛堝寘鎷垹闄よ瘉涔﹀拰鍏抽棴娴嬭瘯绛惧悕锛夛細
     .\uninstall.ps1 -CleanAll

------------------------------------------------------------
  楠岃瘉瀹夎
------------------------------------------------------------

閲嶅惎鍚庡湪 PowerShell 涓繍琛岋細
  Get-PnpDevice -FriendlyName "*Virtual HID*"

鐘舵€佸簲鏄剧ず OK銆?

------------------------------------------------------------
  娉ㄦ剰浜嬮」
------------------------------------------------------------

- 鏈┍鍔ㄤ娇鐢ㄦ祴璇曠鍚嶏紝瀹夎鍚庢闈㈠彸涓嬭浼氬嚭鐜?
  "娴嬭瘯妯″紡" 姘村嵃锛岃繖鏄?Windows 娴嬭瘯绛惧悕妯″紡鐨勬甯歌〃鐜般€?

- 浠呴€傜敤浜?Windows 10/11 x64 鍙?Windows Server 2016+銆?

- 涓嶈鍦ㄧ敓浜х幆澧冧娇鐢ㄦ祴璇曠鍚嶉┍鍔ㄣ€?

============================================================
"@ | Out-File -Encoding UTF8 (Join-Path $StageDir 'README.txt')

# Create ZIP
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path "$StageDir\*" -DestinationPath $ZipPath -CompressionLevel Optimal

$size = [math]::Round((Get-Item $ZipPath).Length / 1024, 1)
Write-Host ""
Write-Host "Build Complete!" -ForegroundColor Green
Write-Host "Output: $ZipPath ($size KB)" -ForegroundColor Green
Write-Host ""
Write-Host "Usage:" -ForegroundColor Cyan
Write-Host "1. Copy zip to target"
Write-Host "2. Run install.ps1"
Write-Host "3. Reboot"?


