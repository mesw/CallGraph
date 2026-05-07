# build-windows.ps1 — configure and build CallGraph (Release) on Windows with MSVC
param(
    [string]$Preset = "windows-msvc-release"
)

$ErrorActionPreference = "Stop"

Write-Host "==> Configuring with preset: $Preset"
cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Building..."
cmake --build --preset $Preset --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Build complete."
