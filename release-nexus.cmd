@echo off
setlocal
where pwsh >nul 2>&1
if errorlevel 1 (
  echo PowerShell 7 is required. Install with: winget install --id Microsoft.PowerShell -e
  exit /b 1
)
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\release-stream-control.ps1" %*
exit /b %errorlevel%
