param([Parameter(Mandatory=$true)][string]$ObsRoot)

$ErrorActionPreference = "Continue"
$report = Join-Path $env:TEMP "T0G-Stream-Control-Diagnostics.txt"
$lines = New-Object System.Collections.Generic.List[string]
function Log([string]$s="") { $lines.Add($s); Write-Host $s }
function Hex16([byte[]]$b,[int]$o){ [BitConverter]::ToUInt16($b,$o) }
function Hex32([byte[]]$b,[int]$o){ [BitConverter]::ToUInt32($b,$o) }

function Get-PEInfo([string]$Path) {
  $r=[ordered]@{Path=$Path;Exists=$false;ValidPE=$false;Machine="";Bits="";Error=""}
  if(!(Test-Path $Path)){return [pscustomobject]$r}
  $r.Exists=$true
  try {
    $b=[IO.File]::ReadAllBytes($Path)
    if($b.Length -lt 256 -or $b[0]-ne 0x4D -or $b[1]-ne 0x5A){$r.Error="Not MZ";return [pscustomobject]$r}
    $pe=[BitConverter]::ToInt32($b,0x3C)
    if($pe+26 -ge $b.Length -or $b[$pe]-ne 0x50 -or $b[$pe+1]-ne 0x45){$r.Error="Invalid PE signature";return [pscustomobject]$r}
    $m=Hex16 $b ($pe+4); $magic=Hex16 $b ($pe+24)
    $r.Machine=("0x{0:X4}" -f $m)
    $r.Bits=if($m -eq 0x8664 -and $magic -eq 0x20B){"x64"}elseif($m -eq 0x14C){"x86"}elseif($m -eq 0xAA64){"ARM64"}else{"UNKNOWN"}
    $r.ValidPE=$true
  } catch {$r.Error=$_.Exception.Message}
  [pscustomobject]$r
}

function Get-ImportedDlls([string]$Path) {
  $result=@()
  try {
    $b=[IO.File]::ReadAllBytes($Path); $pe=[BitConverter]::ToInt32($b,0x3C)
    $sections=Hex16 $b ($pe+6); $opt=$pe+24; $magic=Hex16 $b $opt
    $dd=if($magic -eq 0x20B){$opt+112}else{$opt+96}
    $importRva=Hex32 $b ($dd+8)
    if($importRva -eq 0){return @()}
    $sec=$opt+(Hex16 $b ($pe+20))
    function RvaToOffset([uint32]$rva) {
      for($i=0;$i -lt $sections;$i++){
        $s=$sec+40*$i; $vs=Hex32 $b ($s+8); $va=Hex32 $b ($s+12); $rawSize=Hex32 $b ($s+16); $raw=Hex32 $b ($s+20)
        $span=[Math]::Max($vs,$rawSize)
        if($rva -ge $va -and $rva -lt ($va+$span)){return [int]($raw+($rva-$va))}
      }; return -1
    }
    $off=RvaToOffset $importRva
    while($off -ge 0 -and $off+20 -le $b.Length){
      $nameRva=Hex32 $b ($off+12); if($nameRva -eq 0){break}
      $no=RvaToOffset $nameRva; if($no -lt 0){break}
      $chars=New-Object Collections.Generic.List[byte]
      while($no -lt $b.Length -and $b[$no]-ne 0){$chars.Add($b[$no]);$no++}
      $n=[Text.Encoding]::ASCII.GetString($chars.ToArray())
      if($n){$result += $n}; $off+=20
    }
  } catch { Log ("IMPORT PARSE ERROR: "+$_.Exception.Message) }
  @($result | Sort-Object -Unique)
}

$obsBin=Join-Path $ObsRoot "bin\64bit"
$pluginDir=Join-Path $ObsRoot "obs-plugins\64bit"
$plugin=Join-Path $pluginDir "t0g-stream-control.dll"
Log "T0G Stream Control - Deep Loader Diagnostics"
Log ("Generated: "+(Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
$processArch = if([Environment]::Is64BitProcess){"x64"}else{"x86"}
Log ("PowerShell process: "+$processArch)
Log ("Windows OS is 64-bit: "+[Environment]::Is64BitOperatingSystem)
Log ("Windows: "+[Environment]::OSVersion.VersionString)
if(-not [Environment]::Is64BitProcess -and [Environment]::Is64BitOperatingSystem) {
  Log "PINPOINT: Diagnostic is running in 32-bit PowerShell. A 32-bit process cannot LoadLibrary 64-bit OBS/Qt/T0G DLLs and will produce Win32 error 193."
}
Log ("OBS root: "+$ObsRoot)
Log ("OBS exe: "+(Join-Path $obsBin "obs64.exe"))
Log ""

$searchDirs=@($pluginDir,$obsBin,(Join-Path $ObsRoot "data\obs-plugins\t0g-stream-control")) | Where-Object {Test-Path $_}
$system32=[Environment]::GetFolderPath("System")
$searchDirs += $system32
Log "=== PE ARCHITECTURE ==="
foreach($p in @($plugin,(Join-Path $obsBin "obs64.exe"),(Join-Path $obsBin "obs.dll"),(Join-Path $obsBin "obs-frontend-api.dll"),(Join-Path $obsBin "Qt6Core.dll"),(Join-Path $obsBin "Qt6Gui.dll"),(Join-Path $obsBin "Qt6Widgets.dll"),(Join-Path $obsBin "Qt6Network.dll"))){
  $i=Get-PEInfo $p
  Log ("{0,-5} {1,-6} {2}" -f $(if($i.Exists){"FOUND"}else{"MISS"}),$i.Bits,$p)
  if($i.Error){Log ("      PE ERROR: "+$i.Error)}
}
Log ""

Log "=== DIRECT IMPORTS + RESOLUTION ==="
$imports=Get-ImportedDlls $plugin
if(!$imports.Count){Log "No imports parsed (unexpected)."}
$resolved=@()
foreach($name in $imports){
  $found=$null
  foreach($d in $searchDirs){$candidate=Join-Path $d $name;if(Test-Path $candidate){$found=$candidate;break}}
  if(!$found){
    Log ("MISSING  "+$name)
  } else {
    $pi=Get-PEInfo $found
    $status=if($pi.Bits -eq "x64" -or $name -match '^api-ms-win-|^KERNEL32|^USER32|^ADVAPI32|^SHELL32|^OLE32|^WS2_32'){"OK"}else{"CHECK"}
    Log ("{0,-7} {1,-6} {2} -> {3}" -f $status,$pi.Bits,$name,$found)
    $resolved += $found
  }
}
Log ""

$native=@'
using System;
using System.Runtime.InteropServices;
public static class T0GDeepLoader {
 [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryEx(string f,IntPtr h,uint flags);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool FreeLibrary(IntPtr h);
 [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] public static extern IntPtr AddDllDirectory(string p);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool SetDefaultDllDirectories(uint f);
}
'@
Add-Type $native -ErrorAction SilentlyContinue
$DEFAULT=0x1000;$USER=0x400
[T0GDeepLoader]::SetDefaultDllDirectories($DEFAULT -bor $USER)|Out-Null
foreach($d in $searchDirs){[T0GDeepLoader]::AddDllDirectory($d)|Out-Null}

if(-not [Environment]::Is64BitProcess -and [Environment]::Is64BitOperatingSystem) {
  Log ""
  Log "=== LOAD TEST SKIPPED ==="
  Log "The diagnostic host is 32-bit, so loading x64 OBS dependencies here would create false Win32 193 failures."
  Log "Re-run this script with 64-bit PowerShell (System32\\WindowsPowerShell\\v1.0\\powershell.exe)."
  $lines|Set-Content $report -Encoding UTF8
  Start-Process notepad.exe -ArgumentList ('"'+$report+'"')
  exit 193
}

Log "=== INDIVIDUAL DIRECT DEPENDENCY LOAD TEST ==="
foreach($dep in $resolved){
  $h=[T0GDeepLoader]::LoadLibraryEx($dep,[IntPtr]::Zero,$DEFAULT -bor $USER)
  if($h -eq [IntPtr]::Zero){
    $e=[Runtime.InteropServices.Marshal]::GetLastWin32Error();$m=(New-Object ComponentModel.Win32Exception($e)).Message
    Log ("FAIL ["+$e+"] "+$dep+" :: "+$m)
  } else { Log ("PASS      "+$dep);[T0GDeepLoader]::FreeLibrary($h)|Out-Null }
}
Log ""

Log "=== PLUGIN LOAD TEST ==="
$h=[T0GDeepLoader]::LoadLibraryEx($plugin,[IntPtr]::Zero,$DEFAULT -bor $USER)
if($h -eq [IntPtr]::Zero){
  $e=[Runtime.InteropServices.Marshal]::GetLastWin32Error();$m=(New-Object ComponentModel.Win32Exception($e)).Message
  Log ("PLUGIN FAIL ["+$e+"] "+$m)
  if($e -eq 193){Log "PINPOINT: ERROR_BAD_EXE_FORMAT. Look above for x86/ARM64/invalid PE dependencies in this x64 OBS process."}
  elseif($e -eq 126){Log "PINPOINT: ERROR_MOD_NOT_FOUND. Look above for MISSING imports or a dependency whose own load test fails."}
  elseif($e -eq 127){Log "PINPOINT: ERROR_PROC_NOT_FOUND. A DLL was found but an imported symbol is absent/incompatible."}
} else {Log "PLUGIN PASS: Windows accepted the plugin image.";[T0GDeepLoader]::FreeLibrary($h)|Out-Null}

Log ""
Log "=== INSTALLED T0G FILES ==="
Get-ChildItem (Join-Path $ObsRoot "obs-plugins\64bit") -Filter "t0g*" -ErrorAction SilentlyContinue | ForEach-Object {Log ($_.FullName+" | "+$_.Length+" bytes | "+(Get-FileHash $_.FullName -Algorithm SHA256).Hash)}
Log ""
Log ("REPORT: "+$report)
$lines|Set-Content $report -Encoding UTF8
Start-Process notepad.exe -ArgumentList ('"'+$report+'"')
