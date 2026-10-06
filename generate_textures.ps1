Add-Type -AssemblyName System.Drawing

# 1. soft_shadow.png (128x128 極めて滑らかなガウシアンソフトフェードAO影)
$w = 128
$h = 128
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$cx = $w / 2.0
$cy = $h / 2.0
$radius = 62.0
$sigma = 24.0

for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
        $dx = $x - $cx
        $dy = $y - $cy
        $dist = [Math]::Sqrt($dx * $dx + $dy * $dy)
        if ($dist -ge $radius) {
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        } else {
            # ガウシアン減衰 + 外周フェードアウト
            $gauss = [Math]::Exp(-($dist * $dist) / (2.0 * $sigma * $sigma))
            $fade = 1.0 - ($dist / $radius)
            $fade = $fade * $fade * (3.0 - 2.0 * $fade)
            $alpha = [int](220.0 * $gauss * $fade)
            if ($alpha -lt 0) { $alpha = 0 }
            if ($alpha -gt 255) { $alpha = 255 }
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 0, 0, 0))
        }
    }
}
$bmp.Save("assets/textures/soft_shadow.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "Updated assets/textures/soft_shadow.png"

# 2. crash_scorch.png (256x256 墜落激突の焦げ跡・デカール)
$w = 256
$h = 256
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$cx = $w / 2.0
$cy = $h / 2.0
$rand = New-Object System.Random(42)

for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
        $dx = ($x - $cx) / 1.6
        $dy = ($y - $cy)
        $dist = [Math]::Sqrt($dx * $dx + $dy * $dy)
        $noise = ($rand.NextDouble() - 0.5) * 15.0
        $effDist = $dist + $noise
        if ($effDist -ge 100.0) {
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        } else {
            $t = $effDist / 100.0
            $alpha = [int](200 * (1.0 - $t) * (1.0 - $t))
            if ($alpha -lt 0) { $alpha = 0 }
            if ($alpha -gt 255) { $alpha = 255 }
            $shade = [int](20 + 20 * $t)
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, $shade, $shade, $shade))
        }
    }
}
$bmp.Save("assets/textures/crash_scorch.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "Updated assets/textures/crash_scorch.png"

# 3. tunnel_wall.png (512x512 コンクリート & 金属パネル & ハザードストライプ)
$w = 512
$h = 512
$bmp = New-Object System.Drawing.Bitmap($w, $h)

for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
        $noise = ($rand.NextDouble() - 0.5) * 16.0
        $base = 80.0 + $noise
        
        # 128px周期のパネル目地（黒いスリットライン）
        $isGridX = (($x % 128) -le 3) -or (($x % 128) -ge 125)
        $isGridY = (($y % 128) -le 3) -or (($y % 128) -ge 125)
        if ($isGridX -or $isGridY) {
            $base = $base - 50.0
        }
        
        # リベット
        $relX = $x % 128
        $relY = $y % 128
        $isRivet = ($relX -eq 14 -or $relX -eq 114) -and ($relY -eq 14 -or $relY -eq 114)
        if ($isRivet) {
            $base = $base + 65.0
        }
        
        # 中央の黄黒警告ハザードストライプ帯 (Y=240〜272)
        $isHazard = ($y -ge 240) -and ($y -le 272)
        if ($isHazard) {
            $stripe = (($x + $y) % 32) -lt 16
            if ($stripe) {
                # 黄色ストライプ
                $r = 210
                $g = 180
                $b = 30
                $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $r, $g, $b))
                continue
            } else {
                # 黒ストライプ
                $r = 25
                $g = 25
                $b = 28
                $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $r, $g, $b))
                continue
            }
        }

        $c = [int]$base
        if ($c -lt 12) { $c = 12 }
        if ($c -gt 240) { $c = 240 }
        
        $r = [int]($c * 0.92)
        $g = [int]($c * 0.95)
        $b = $c
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $r, $g, $b))
    }
}
$bmp.Save("assets/textures/tunnel_wall.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "Updated assets/textures/tunnel_wall.png"
