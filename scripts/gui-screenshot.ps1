Add-Type -AssemblyName System.Windows.Forms,System.Drawing

$src = @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@
Add-Type -TypeDefinition $src -Language CSharp

$proc = Start-Process -FilePath ".\build\src\gui\Debug\vhid-gui.exe" -PassThru
Start-Sleep -Seconds 2
$proc.Refresh()
$h = $proc.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Host "no main window"; exit 1 }
$r = New-Object Win+RECT
[Win]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L
$hh = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $hh))
$bmp.Save("$PSScriptRoot\..\build\gui-screenshot.png")
$g.Dispose(); $bmp.Dispose()
Write-Host "saved $w x $hh"
Stop-Process -Id $proc.Id -Force
