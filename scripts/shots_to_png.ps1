<#
Convert the emulator's panel screenshots (BMP, written by Ctrl+S or the scripted
touch runner's 'shot' step) to PNG, so they can be viewed directly.

    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\shots_to_png.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\shots_to_png.ps1 -Dir screenshots -Out build\png
#>
param(
  [string]$Dir = "screenshots",
  [string]$Out = ""
)
Add-Type -AssemblyName System.Drawing
if (-not (Test-Path $Dir)) { Write-Host "no $Dir"; exit 1 }
$target = if ($Out) { $Out } else { $Dir }
if ($Out -and -not (Test-Path $Out)) { New-Item -ItemType Directory -Path $Out | Out-Null }
Get-ChildItem (Join-Path $Dir "*.bmp") | Sort-Object Name | ForEach-Object {
  $img = [System.Drawing.Image]::FromFile($_.FullName)
  $outPath = Join-Path $target ($_.BaseName + ".png")
  $img.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
  $img.Dispose()
  Write-Host $outPath
}
