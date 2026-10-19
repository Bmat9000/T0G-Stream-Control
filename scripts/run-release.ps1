# Compatible with Windows PowerShell so missing PowerShell 7 is logged too.
[CmdletBinding()]
param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$log = Join-Path $repoRoot 'notepad.txt'
$code = 1
try {
    "T0G STREAM CONTROL RELEASE`r`nTime: $([DateTime]::UtcNow.ToString('o'))`r`n" |
        Set-Content -Encoding UTF8 $log
    $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
    if (-not $pwsh) {
        throw 'PowerShell 7 is required. Install with: winget install --id Microsoft.PowerShell -e'
    }
    $releaseArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'release-stream-control.ps1'))
    if ($BuildOnly) { $releaseArgs += '-BuildOnly' }
    # Windows PowerShell treats native stderr as error records. Keep capturing it
    # without aborting before the child process returns its real exit status.
    $ErrorActionPreference = 'Continue'
    & $pwsh.Source @releaseArgs 2>&1 | ForEach-Object { $_.ToString() } |
        Tee-Object -FilePath $log -Append
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
} catch {
    "RELEASE FAILED: $($_.Exception.Message)" | Tee-Object -FilePath $log -Append
    $code = 1
}
if ($code -ne 0) {
    "`r`nRELEASE FAILED (exit $code). Send this notepad.txt file for troubleshooting." |
        Tee-Object -FilePath $log -Append
    try { Start-Process notepad.exe -ArgumentList ('"' + $log + '"') } catch { Write-Host "Open log: $log" }
} else {
    Write-Host "Release log saved: $log"
}
exit $code
