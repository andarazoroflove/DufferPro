Add-Type -AssemblyName System.Drawing
$names = @("screenshot_gba_splash", "screenshot_gba_play", "screenshot_gba_carl_dynamite", "screenshot_gba_carl_hose")
$outDir = "C:\Users\adam\.gemini\antigravity\brain\1c635384-03d5-4087-b256-c838f707f215"
foreach ($name in $names) {
    $src = "$name.bmp"
    $dst = Join-Path $outDir "$name.png"
    $bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $src).Path)
    $bmp.Save($dst, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Converted $src to $dst"
}
