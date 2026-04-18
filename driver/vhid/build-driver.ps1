# build-driver.ps1 — Build vhid.sys KMDF driver using direct cl.exe + link.exe
param([string]$Config = "Release")
$ErrorActionPreference = 'Stop'

# --- Paths ---
$WDK_ROOT    = 'D:\Windows Kits\10'
$SDK_VERSION = '10.0.26100.0'
$KMDF_VER    = '1.33'
$VS_ROOT     = 'D:\Program Files\Microsoft Visual Studio\2022\Community'

$msvcDir = Get-ChildItem "$VS_ROOT\VC\Tools\MSVC" -Directory | Sort-Object Name -Desc | Select-Object -First 1
$MSVC_ROOT = $msvcDir.FullName
$CL   = "$MSVC_ROOT\bin\Hostx64\x64\cl.exe"
$LINK = "$MSVC_ROOT\bin\Hostx64\x64\link.exe"

Write-Host "MSVC: $($msvcDir.Name)"

$MSVC_LIB = "$MSVC_ROOT\lib\x64"
$MSVC_INC = "$MSVC_ROOT\include"

$KM_INC     = "$WDK_ROOT\Include\$SDK_VERSION\km"
$SHARED_INC = "$WDK_ROOT\Include\$SDK_VERSION\shared"
$UCRT_INC   = "$WDK_ROOT\Include\$SDK_VERSION\ucrt"
$WDF_INC    = "$WDK_ROOT\Include\wdf\kmdf\$KMDF_VER"
$KM_LIB     = "$WDK_ROOT\Lib\$SDK_VERSION\km\x64"
$WDF_LIB    = "$WDK_ROOT\Lib\wdf\kmdf\x64\$KMDF_VER"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$OutDir    = "$ScriptDir\..\..\build\driver\$Config"
$ObjDir    = "$OutDir\obj"
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
New-Item -ItemType Directory -Path $ObjDir -Force | Out-Null

# --- Compile ---
$Sources = @('Driver.cpp', 'Vhf.cpp')
$Objects = @()

foreach ($src in $Sources) {
    $srcPath = "$ScriptDir\$src"
    $objPath = "$ObjDir\$([IO.Path]::GetFileNameWithoutExtension($src)).obj"
    $Objects += "`"$objPath`""

    Write-Host "`n=== Compiling $src ==="
    & $CL /c /nologo /W4 /WX- /GS- /Gz /Zp8 /Gy /Zc:wchar_t /Zc:forScope /Zc:inline /GR- /EHs-c- /kernel /utf-8 `
        /D_AMD64_ /DAMD64 /D_WIN64 /DNDEBUG /DNTDDI_VERSION=0x0A00000C `
        /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DWINNT=1 `
        /DPOOL_NX_OPTIN=1 /DKMDF_VERSION_MAJOR=1 /DKMDF_VERSION_MINOR=33 `
        /I"$KM_INC" /I"$SHARED_INC" /I"$UCRT_INC" /I"$WDF_INC" /I"$MSVC_INC" /I"$ScriptDir" `
        /O2 /Oi `
        /Fo"$objPath" "$srcPath" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation of $src failed (exit code $LASTEXITCODE)"
        exit 1
    }
}

# --- Link ---
$SYS_OUT = "$OutDir\vhid.sys"
Write-Host "`n=== Linking vhid.sys ==="
& $LINK /nologo /DRIVER /KERNEL `
    /SUBSYSTEM:NATIVE,10.00 `
    /NODEFAULTLIB `
    /ENTRY:FxDriverEntry `
    /OUT:"$SYS_OUT" `
    /LIBPATH:"$KM_LIB" `
    /LIBPATH:"$WDF_LIB" `
    /LIBPATH:"$MSVC_LIB" `
    ntoskrnl.lib hal.lib wmilib.lib `
    vhfkm.lib `
    WdfLdr.lib WdfDriverEntry.lib `
    BufferOverflowFastFailK.lib `
    libcmt.lib wdmsec.lib `
    @Objects 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Error "Link failed (exit code $LASTEXITCODE)"
    exit 1
}

Write-Host "`nSUCCESS: $SYS_OUT"
Write-Host "Size: $((Get-Item $SYS_OUT).Length) bytes"
