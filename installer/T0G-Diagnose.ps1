param(
    [Parameter(Mandatory=$true)]
    [string]$ObsRoot
)

$ErrorActionPreference = "Continue"
$lines = New-Object System.Collections.Generic.List[string]
function Add-Line([string]$s) { $lines.Add($s); Write-Host $s }

$obsBin = Join-Path $ObsRoot "bin\64bit"
$plugin = Join-Path $ObsRoot "obs-plugins\64bit\t0g-stream-control.dll"
$report = Join-Path $env:TEMP "T0G-Stream-Control-Diagnostics.txt"

Add-Line "T0G Stream Control Diagnostics"
Add-Line ("Generated: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
Add-Line ("OBS root: " + $ObsRoot)
Add-Line ""

$checks = @(
    $plugin,
    (Join-Path $obsBin "obs64.exe"),
    (Join-Path $obsBin "obs.dll"),
    (Join-Path $obsBin "obs-frontend-api.dll"),
    (Join-Path $obsBin "Qt6Core.dll"),
    (Join-Path $obsBin "Qt6Gui.dll"),
    (Join-Path $obsBin "Qt6Widgets.dll"),
    (Join-Path $obsBin "Qt6Network.dll")
)

foreach ($file in $checks) {
    if (Test-Path $file) {
        Add-Line ("OK      " + $file)
    } else {
        Add-Line ("MISSING " + $file)
    }
}

Add-Line ""
if (Test-Path (Join-Path $obsBin "obs64.exe")) {
    $v = (Get-Item (Join-Path $obsBin "obs64.exe")).VersionInfo.FileVersion
    Add-Line ("OBS executable version: " + $v)
}
if (Test-Path $plugin) {
    Add-Line ("Plugin SHA256: " + (Get-FileHash $plugin -Algorithm SHA256).Hash)
}

$source = @'
using System;
using System.Runtime.InteropServices;
public static class T0GLoader {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern IntPtr LoadLibraryEx(string file, IntPtr hFile, uint flags);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool FreeLibrary(IntPtr module);
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern IntPtr AddDllDirectory(string path);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool SetDefaultDllDirectories(uint flags);
}
'@
Add-Type $source -ErrorAction SilentlyContinue

$LOAD_LIBRARY_SEARCH_DEFAULT_DIRS = 0x00001000
$LOAD_LIBRARY_SEARCH_USER_DIRS = 0x00000400
[T0GLoader]::SetDefaultDllDirectories($LOAD_LIBRARY_SEARCH_DEFAULT_DIRS -bor $LOAD_LIBRARY_SEARCH_USER_DIRS) | Out-Null

$dirs = @($obsBin, (Join-Path $ObsRoot "obs-plugins\64bit")) | Where-Object { Test-Path $_ }
foreach ($dir in $dirs) {
    [T0GLoader]::AddDllDirectory($dir) | Out-Null
    Add-Line ("DLL search: " + $dir)
}

Add-Line ""
if (-not (Test-Path $plugin)) {
    Add-Line "RESULT: plugin DLL is not installed."
} else {
    $h = [T0GLoader]::LoadLibraryEx($plugin, [IntPtr]::Zero, $LOAD_LIBRARY_SEARCH_DEFAULT_DIRS -bor $LOAD_LIBRARY_SEARCH_USER_DIRS)
    if ($h -eq [IntPtr]::Zero) {
        $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        $message = (New-Object ComponentModel.Win32Exception($code)).Message
        Add-Line ("RESULT: FAILED - Win32 error " + $code + ": " + $message)
    } else {
        Add-Line "RESULT: SUCCESS - Windows loaded t0g-stream-control.dll."
        [T0GLoader]::FreeLibrary($h) | Out-Null
    }
}

$lines | Set-Content -Path $report -Encoding UTF8
Start-Process notepad.exe -ArgumentList ('"' + $report + '"')
