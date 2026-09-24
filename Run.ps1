Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Get-Process PencilBridge -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 150

& (Join-Path $PSScriptRoot "Ensure-Https.ps1")

$paths = @(
    (Join-Path $PSScriptRoot "build\PencilBridge.exe"),
    (Join-Path $PSScriptRoot "build\Release\PencilBridge.exe")
)

$exe = $paths | Where-Object { Test-Path $_ } | Select-Object -First 1
if (!$exe) {
    throw "PencilBridge.exe not found. Run .\Build.ps1 first."
}

Start-Process -FilePath $exe -WorkingDirectory $PSScriptRoot
Write-Host "Started: $exe"
