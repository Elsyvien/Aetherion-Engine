param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")),
  [string]$AssetsDir = "assets"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = (Resolve-Path $RepoRoot).Path
$assetsPath = Join-Path $repo $AssetsDir

if (-not (Test-Path $assetsPath)) {
  throw "Assets directory not found: $assetsPath"
}

function Get-AssetType([string]$path) {
  $ext = [System.IO.Path]::GetExtension($path).ToLowerInvariant()
  switch ($ext) {
    ".png" { return "Texture" }
    ".jpg" { return "Texture" }
    ".jpeg" { return "Texture" }
    ".tga" { return "Texture" }
    ".bmp" { return "Texture" }
    ".gif" { return "Texture" }
    ".dds" { return "Texture" }
    ".ktx" { return "Texture" }
    ".ktx2" { return "Texture" }

    ".gltf" { return "Mesh" }
    ".glb" { return "Mesh" }
    ".obj" { return "Mesh" }
    ".fbx" { return "Mesh" }
    ".dae" { return "Mesh" }

    ".wav" { return "Audio" }
    ".mp3" { return "Audio" }
    ".ogg" { return "Audio" }
    ".flac" { return "Audio" }
    ".aiff" { return "Audio" }

    ".lua" { return "Script" }
    ".py" { return "Script" }
    ".js" { return "Script" }
    ".cs" { return "Script" }

    ".vert" { return "Shader" }
    ".frag" { return "Shader" }
    ".glsl" { return "Shader" }
    ".spv" { return "Shader" }

    ".json" {
      # match AssetRegistry: if path contains /scenes/ then Scene else Other
      if ($path -match "[\\/]scenes[\\/]") { return "Scene" }
      return "Other"
    }

    default { return "Other" }
  }
}

Write-Host "Asset report for: $assetsPath" -ForegroundColor Cyan

$files = Get-ChildItem -Path $assetsPath -Recurse -File -ErrorAction Stop |
  Where-Object { $_.Name -notmatch '^\.' }

if (-not $files -or $files.Count -eq 0) {
  Write-Warning "No files found under assets/."
}

$items = foreach ($f in $files) {
  $rel = $f.FullName.Substring($assetsPath.Length).TrimStart('\','/')

  [pscustomobject]@{
    RelPath = $rel
    Type = (Get-AssetType $f.FullName)
    SizeBytes = $f.Length
    Ext = $f.Extension
  }
}

$byType = $items | Group-Object Type | Sort-Object Name

Write-Host "\nCounts by type:" -ForegroundColor Cyan
$byType | ForEach-Object {
  "{0,-8} {1,6}" -f $_.Name, $_.Count
} | Write-Host

$totalBytes = ($items | Measure-Object -Property SizeBytes -Sum).Sum
if ($null -eq $totalBytes) { $totalBytes = 0 }
$mb = [Math]::Round($totalBytes / 1MB, 2)
Write-Host ("Total: {0} files, {1} MB" -f ($items.Count), $mb) -ForegroundColor Cyan

# Sanity checks
$bootstrap = Join-Path $assetsPath "scenes/bootstrap_scene.json"
if (-not (Test-Path $bootstrap)) {
  Write-Warning "Missing bootstrap scene: assets/scenes/bootstrap_scene.json"
}

$upperExt = @($items | Where-Object { $_.Ext -ne $_.Ext.ToLowerInvariant() })
if ($upperExt.Count -gt 0) {
  Write-Warning ("Found {0} file(s) with uppercase extension (can break case-sensitive tooling later)." -f $upperExt.Count)
  $upperExt | Select-Object -First 10 RelPath, Ext | Format-Table | Out-String | Write-Host
}

# Duplicate filenames (different folders) - useful for ID collisions in some pipelines
$dupes = @($files | Group-Object Name | Where-Object { $_.Count -gt 1 } | Sort-Object Count -Descending)
if ($dupes.Count -gt 0) {
  Write-Warning ("Duplicate filenames detected ({0} group(s))." -f $dupes.Count)
  $dupes | Select-Object -First 10 Name, Count | Format-Table | Out-String | Write-Host
}

Write-Host "\nTop 20 largest files:" -ForegroundColor Cyan
$items | Sort-Object SizeBytes -Descending | Select-Object -First 20 RelPath, Type, SizeBytes |
  Format-Table | Out-String | Write-Host
