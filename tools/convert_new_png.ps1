Add-Type -AssemblyName System.Drawing
$bmps = @('screenshot_splash_v3', 'screenshot_setup_screen', 'screenshot_bushwood_replay', 'screenshot_la_playa', 'screenshot_nocturnal_green', 'screenshot_sneak_on_ranger')
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

