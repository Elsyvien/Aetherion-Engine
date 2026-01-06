# Build a hot-reload module for Aetherion Engine
param(
    [Parameter(Mandatory=$true, HelpMessage="Name of the module to build (e.g., RotationBehavior)")]
    [string]$ModuleName
)

$ErrorActionPreference = "Stop"

$ModuleDir = "Engine/Scripting/examples"
$BuildDir = "build-mingw"
$OutputDir = "$BuildDir/modules"

# Check if source file exists
$SourceFile = "$ModuleDir/$ModuleName.cpp"
if (-not (Test-Path $SourceFile)) {
    Write-Host "Error: Module source file not found: $SourceFile" -ForegroundColor Red
    exit 1
}

Write-Host "Building module: $ModuleName" -ForegroundColor Cyan
Write-Host "Source: $SourceFile"

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Setup environment
$QtPath = "C:/Qt/Tools/mingw1310_64/bin"
if (Test-Path $QtPath) {
    $env:PATH = "$QtPath;$env:PATH"
}

$OutputFile = "$OutputDir/$ModuleName.dll"

Write-Host "Compiler: g++"
Write-Host "Output: $OutputFile"
Write-Host ""

# Build command
$BuildCmd = @(
    "g++",
    "-shared",
    "-std=c++17",
    "-O2",
    "-I`"Engine/`"",
    "`"$SourceFile`"",
    "-o", "`"$OutputFile`"",
    "-L`"$BuildDir`"",
    "-lAetherionRuntime"
)

Write-Host "Executing: $($BuildCmd -join ' ')" -ForegroundColor Gray
Write-Host ""

& $BuildCmd[0] $BuildCmd[1..($BuildCmd.Length-1)]

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "✓ Module built successfully: $OutputFile" -ForegroundColor Green
    Write-Host ""
    Write-Host "To use the module:" -ForegroundColor Yellow
    Write-Host "1. Start the Aetherion Editor"
    Write-Host "2. Open the Logic Copilot panel"
    Write-Host "3. Click 'Compile & Load' or use the ModuleLoader API"
} else {
    Write-Host ""
    Write-Host "✗ Build failed" -ForegroundColor Red
    exit 1
}
