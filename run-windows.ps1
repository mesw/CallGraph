# run-windows.ps1 — launch CallGraph pointing at $env:CALLGRAPH_SOURCE_ROOT
param(
    [string]$SourceRoot = $env:CALLGRAPH_SOURCE_ROOT,
    [string]$Preset = "windows-msvc-release"
)

$ErrorActionPreference = "Stop"

if (-not $SourceRoot) {
    Write-Error "Set CALLGRAPH_SOURCE_ROOT or pass -SourceRoot <path>"
    exit 1
}

$exePath = Join-Path $PSScriptRoot "build\release\CallGraph\Release\CallGraph.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found at $exePath — run build-windows.ps1 first"
    exit 1
}

Write-Host "==> Launching CallGraph with source root: $SourceRoot"
& $exePath --source-root $SourceRoot
