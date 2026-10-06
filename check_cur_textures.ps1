Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path "Resources\models\m21.png"))
Write-Host "m21.png size: $($bmp.Width)x$($bmp.Height)"
$bmp.Dispose()
$bmp2 = [System.Drawing.Bitmap]::FromFile((Resolve-Path "Resources\models\m21wg.png"))
Write-Host "m21wg.png size: $($bmp2.Width)x$($bmp2.Height)"
$bmp2.Dispose()
