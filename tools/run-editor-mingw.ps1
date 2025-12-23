param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")),
  [string]$QtToolsBin = "C:/Qt/Tools/mingw1310_64/bin",
  [string]$QtBin = "C:/Qt/6.9.1/mingw_64/bin",
  [string]$QtPluginPath = "C:/Qt/6.9.1/mingw_64/plugins",
  [string]$BuildDir = "build-mingw",
  [string]$ExeName = "AetherionEditor.exe"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = (Resolve-Path $RepoRoot).Path
$build = Join-Path $repo $BuildDir
$exe = Join-Path $build $ExeName

if (-not (Test-Path $exe)) {
  throw "Executable not found: $exe. Build first (CMake: Build (MinGW))."
}

$env:PATH = "$QtToolsBin;$QtBin;" + $env:PATH
$env:QT_PLUGIN_PATH = $QtPluginPath

Write-Host "Running: $exe" -ForegroundColor Cyan
Push-Location $build
try {
  & $exe
  exit $LASTEXITCODE
}
finally {
  Pop-Location
}
