#Requires -Version 7.2
[CmdletBinding()]
param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'Run this release command on Windows x64.' }
$source = Split-Path $PSScriptRoot -Parent
$repoRoot = $source
Set-Location $source
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
foreach ($tool in @('git', 'gh', 'cmake', 'cargo')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Missing $tool. See README > Local Windows releases for installation commands."
    }
}
$iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Install Inno Setup 6. See README > Local Windows releases.' }
Run gh @('auth', 'status')
$sha = (& git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Run from the Stream Control repository.' }
if (& git status --porcelain) { throw 'Commit or stash local changes before building a release.' }
if (-not $BuildOnly) {
    $branch = (& git branch --show-current).Trim()
    if ($branch -ne 'main') { throw 'Public releases must be built from main. Use -BuildOnly for other branches.' }
    Run git @('fetch', 'origin', 'main')
    $remote = (& git rev-parse origin/main).Trim()
    if ($sha -ne $remote) { throw 'Update main with git pull --ff-only before releasing.' }
}
$notes = Get-Content -Raw "$source/RELEASE_NOTES.md"
foreach ($section in @('Added','Improved','Fixed')) {
    if ($notes -notmatch "(?m)^## $section\s*$") { throw "Missing release notes section: $section" }
}
$build = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss') + '-' + $sha.Substring(0,7)
$work = Join-Path $env:TEMP "t0g-release-$build-$([guid]::NewGuid().ToString('N').Substring(0,6))"
New-Item -ItemType Directory -Path $work | Out-Null
$log = Join-Path $work 'release.log'
Start-Transcript -Path $log | Out-Null
try {
    $snapshot = Join-Path $work 'source'
    Run git @('clone', '--no-hardlinks', '--no-checkout', $repoRoot, $snapshot)
    Run git @('-C', $snapshot, 'checkout', '--detach', $sha)
    $source = $snapshot
    $template = Join-Path $work 'template'
    Run git @('clone', '--recurse-submodules', 'https://github.com/obsproject/obs-plugintemplate.git', $template)
    Set-Location $template
    Run git @('checkout', '3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc')
    Run git @('submodule', 'update', '--init', '--recursive')
    # Use an installed Windows SDK rather than the CI runner's exact SDK version.
    $presets = Get-Content -Raw CMakePresets.json
    $presets.Replace('x64,version=10.0.22621', 'x64') | Set-Content -Encoding utf8 CMakePresets.json
    Remove-Item -Recurse -Force src
    Copy-Item -Recurse "$source/obs-plugin/src" src
    Copy-Item "$source/obs-plugin/ci/CMakeLists.txt" CMakeLists.txt -Force
    Copy-Item "$source/obs-plugin/ci/buildspec.json" buildspec.json -Force
    Run git @('clone', 'https://github.com/PirateTok/live-c.git', 'piratetok-live-c')
    Push-Location piratetok-live-c
    try {
        Run git @('checkout', '55c94d2b47d35a88905e4a23be5e384408943bcf')
        Run cargo @('build', '--release', '--locked')
    } finally { Pop-Location }
    New-Item -ItemType Directory -Path 'local-chat-runtime' | Out-Null
    Copy-Item 'piratetok-live-c/target/release/piratetok.dll' 'local-chat-runtime/piratetok.dll'
    $oldCI = $env:CI
    $env:CI = 'true'
    try {
        Run pwsh @('-NoProfile','-File', '.github/scripts/Build-Windows.ps1', '-Target','x64','-Configuration','RelWithDebInfo')
    } finally { $env:CI = $oldCI }

$ErrorActionPreference = "Stop"
$dll = Get-ChildItem -Path release -Recurse -Filter "t0g-stream-control.dll" | Select-Object -First 1
if (-not $dll) { throw "t0g-stream-control.dll was not produced." }

Write-Host "::group::T0G DLL identity"
Write-Host "DLL: $($dll.FullName)"
Write-Host "Size: $($dll.Length) bytes"
Get-FileHash $dll.FullName -Algorithm SHA256 | Format-List
Write-Host "::endgroup::"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'Install Visual Studio 2022 C++ Build Tools.' }
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dumpbin = Get-ChildItem "$vs/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $dumpbin) { throw "dumpbin.exe not found on the Windows runner." }

Write-Host "::group::T0G DLL headers"
$headers = (& $dumpbin.FullName /headers $dll.FullName | Out-String)
Write-Host $headers
if ($LASTEXITCODE -ne 0) { throw "dumpbin /headers failed." }
if ($headers -notmatch "machine \(x64\)") {
  throw "Plugin is not an x64 DLL. OBS 64-bit requires IMAGE_FILE_MACHINE_AMD64."
}
Write-Host "::endgroup::"

Write-Host "::group::T0G DLL imports"
& $dumpbin.FullName /dependents $dll.FullName
if ($LASTEXITCODE -ne 0) { throw "dumpbin /dependents failed." }
Write-Host "::endgroup::"

Write-Host "::group::T0G full import table"
& $dumpbin.FullName /imports $dll.FullName
if ($LASTEXITCODE -ne 0) { throw "dumpbin /imports failed." }
Write-Host "::endgroup::"

Write-Host "::group::T0G OBS module exports"
$exports = (& $dumpbin.FullName /exports $dll.FullName | Out-String)
Write-Host $exports
foreach ($required in @("obs_module_load", "obs_module_unload", "obs_module_ver", "obs_module_set_pointer")) {
  if ($LASTEXITCODE -ne 0) { throw "dumpbin /exports failed." }
  if ($exports -notmatch [regex]::Escape($required)) {
    throw "Required OBS export '$required' is missing."
  }
}
Write-Host "::endgroup::"
Write-Host "T0G DLL diagnostics passed."

$ErrorActionPreference = "Stop"
$dll = Get-ChildItem -Path release -Recurse -Filter "t0g-stream-control.dll" | Select-Object -First 1
if (-not $dll) { throw "t0g-stream-control.dll was not produced." }

$native = @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class T0GNativeLoader {
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool FreeLibrary(IntPtr hModule);
}
'@
Add-Type $native
$obsDll = Get-ChildItem -Path . -Recurse -Filter "obs.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
$frontendDll = Get-ChildItem -Path . -Recurse -Filter "obs-frontend-api.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $obsDll) { throw "obs.dll could not be located in the build workspace." }
if (-not $frontendDll) { throw "obs-frontend-api.dll could not be located in the build workspace." }

$runtimeDirs = @($obsDll.DirectoryName, $frontendDll.DirectoryName) | Select-Object -Unique

# The OBS dependency bundle contains more than Qt (for example curl/SSL and
# other transitive DLLs used by obs.dll). Add every dependency bin folder,
# not just the directory containing Qt6Core.dll.
$dependencyBinDirs = Get-ChildItem -Path ".deps" -Recurse -Filter "*.dll" -ErrorAction SilentlyContinue |
  ForEach-Object { $_.DirectoryName } |
  Select-Object -Unique
$runtimeDirs = @($runtimeDirs) + @($dependencyBinDirs)
$runtimeDirs = @($runtimeDirs | Where-Object { $_ } | Select-Object -Unique)

foreach ($dir in $runtimeDirs) {
  Write-Host "Adding runtime DLL directory: $dir"
}
$runtimePath = [string]::Join(';', [string[]]$runtimeDirs)
$env:PATH = "$runtimePath;$env:PATH"

# Use normal dependency resolution so PATH is honored for transitive
# OBS/Qt dependencies. LOAD_WITH_ALTERED_SEARCH_PATH can bypass the
# PATH setup above and produce a misleading error 126 in CI.
$handle = [T0GNativeLoader]::LoadLibraryEx($dll.FullName, [IntPtr]::Zero, 0)
if ($handle -eq [IntPtr]::Zero) {
  $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  $message = (New-Object ComponentModel.Win32Exception($code)).Message
  throw "Windows LoadLibraryEx failed for t0g-stream-control.dll. Win32 error ${code}: $message"
}
[T0GNativeLoader]::FreeLibrary($handle) | Out-Null
Write-Host "Windows loader accepted t0g-stream-control.dll."

$pluginDll = Get-ChildItem -Path release -Recurse -Filter "t0g-stream-control.dll" | Select-Object -First 1
if (-not $pluginDll) { throw "t0g-stream-control.dll was not found in the packaged release." }

$payload = "installer-payload"
New-Item -ItemType Directory -Force -Path "$payload/obs-plugins/64bit" | Out-Null
Copy-Item -Force $pluginDll.FullName "$payload/obs-plugins/64bit/t0g-stream-control.dll"

# Qt HTTPS support needs a TLS backend plugin. Bundle the exact backend
# from the OBS 32.1.2 Qt dependency set used for this plugin build.
$tlsPlugins = Get-ChildItem -Path ".deps" -Recurse -Filter "*.dll" -ErrorAction SilentlyContinue |
  Where-Object { $_.DirectoryName -match '[\\/]tls$' -and $_.Name -match '^q.*backend\.dll$' }
if (-not $tlsPlugins) {
  throw "No Qt TLS backend plugins were found in the OBS dependency bundle."
}

New-Item -ItemType Directory -Force -Path "$payload/bin/64bit/tls" | Out-Null
foreach ($tls in $tlsPlugins) {
  Write-Host "Bundling Qt TLS backend: $($tls.FullName)"
  Copy-Item -Force $tls.FullName "$payload/bin/64bit/tls/$($tls.Name)"
}

$localTikTok = "local-chat-runtime/piratetok.dll"
if (-not (Test-Path $localTikTok)) { throw "Local TikTok chat runtime is missing." }
New-Item -ItemType Directory -Force -Path "$payload/bin/64bit" | Out-Null
Copy-Item -Force $localTikTok "$payload/bin/64bit/piratetok.dll"


$pluginData = Get-ChildItem -Path release -Recurse -Directory |
  Where-Object { $_.Name -eq "data" -and $_.Parent.Name -eq "t0g-stream-control" } |
  Select-Object -First 1
if ($pluginData) {
  New-Item -ItemType Directory -Force -Path "$payload/data/obs-plugins/t0g-stream-control" | Out-Null
  Copy-Item -Recurse -Force "$($pluginData.FullName)/*" "$payload/data/obs-plugins/t0g-stream-control/"
}

& $iscc "/DPluginPayload=$((Resolve-Path $payload).Path)" "/DInstallerOutput=$((Resolve-Path release).Path)" "$source/installer/T0G-Stream-Control.iss"
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }

    $out = Join-Path $repoRoot "dist/$build"
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    $installer = Join-Path $out 'T0G-Stream-Control-Setup.exe'
    Copy-Item 'release/T0G-Stream-Control-Setup.exe' $installer
    $date = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm 'UTC'")
    $body = Join-Path $out 'RELEASE_NOTES.md'
    "**Build:** $build`n**Date:** $date`n**Commit:** [$sha](https://github.com/Bmat9000/T0G-Stream-Control/commit/$sha)`n`n$notes" |
        Set-Content -Encoding utf8 $body
    Get-FileHash $installer -Algorithm SHA256 | Format-List | Out-File (Join-Path $out 'SHA256.txt')
    if (-not $BuildOnly) {
        Set-Location $repoRoot
        Run git @('fetch','origin','main')
        if ((& git rev-parse origin/main).Trim() -ne $sha) { throw 'Main changed during build. Installer saved locally; rebuild before publishing.' }
        $tag = "t0g-stream-control-$build"
        # Publish an immutable release first, then update the existing stable download.
        Run gh @('release','create',$tag,'--repo','Bmat9000/T0G-Stream-Control','--target',$sha,'--title',"T0G Stream Control - Build $build",'--notes-file',$body,$installer)
        Run git @('tag','-f','t0g-stream-control-latest',$sha)
        Run git @('push','origin','refs/tags/t0g-stream-control-latest','--force')
        Run gh @('release','view','t0g-stream-control-latest','--repo','Bmat9000/T0G-Stream-Control')
        Run gh @('release','upload','t0g-stream-control-latest',$installer,'--clobber','--repo','Bmat9000/T0G-Stream-Control')
        Run gh @('release','edit','t0g-stream-control-latest','--title',"T0G Stream Control - Build $build",'--notes-file',$body,'--latest','--repo','Bmat9000/T0G-Stream-Control')
    }
    Write-Host "SUCCESS: $installer"
} catch {
    Write-Host "RELEASE FAILED: $($_.Exception.Message)`nFull log: $log" -ForegroundColor Red
    exit 1
} finally {
    Set-Location $repoRoot
    Stop-Transcript | Out-Null
}
