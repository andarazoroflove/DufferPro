Add-Type -AssemblyName System.Drawing
$bmps = @('screenshot_gba_splash', 'screenshot_gba_splash_play_hole', 'screenshot_gba_aiming', 'screenshot_gba_locked', 'screenshot_gba_pinned_flag', 'screenshot_gba_walking', 'screenshot_gba_la_playa', 'screenshot_gba_nocturnal_green', 'screenshot_gba_sneak_on_ranger', 'screenshot_gba_bev_cart', 'screenshot_gba_column_jump')
$artDir = 'C:\Users\adam\.gemini\antigravity\brain\1c635384-03d5-4087-b256-c838f707f215'

foreach ($b in $bmps) {
    $bmpPath = Join-Path (Get-Location) "$b.bmp"
    if (Test-Path $bmpPath) {
        $img = [System.Drawing.Image]::FromFile($bmpPath)
        $pngPath = Join-Path $artDir "$b.png"
        $img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $img.Dispose()
        Write-Host "Converted and copied: $pngPath"
    }
}

