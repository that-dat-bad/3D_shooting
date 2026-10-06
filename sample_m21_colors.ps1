Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path "Resources\models\m21.png"))
Write-Host "m21.png size: $($bmp.Width)x$($bmp.Height)"

# サンプリング: 画像の各領域の色分布
$colors = @{}
for ($y = 0; $y -lt $bmp.Height; $y += 64) {
    for ($x = 0; $x -lt $bmp.Width; $x += 64) {
        $c = $bmp.GetPixel($x, $y)
        $key = "$($c.R),$($c.G),$($c.B),$($c.A)"
        if (-not $colors.ContainsKey($key)) {
            $colors[$key] = 0
        }
        $colors[$key]++
    }
}
Write-Host "Sampled colors in m21.png:"
$colors.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 10 | ForEach-Object {
    Write-Host "  Color ($($_.Key)): count=$($_.Value)"
}
$bmp.Dispose()
