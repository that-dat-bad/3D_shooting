Add-Type -AssemblyName System.Drawing

function CreateFlameTexture {
    $w = 256
    $h = 512
    $bmp = New-Object System.Drawing.Bitmap($w, $h)

    for ($y = 0; $y -lt $h; $y++) {
        $v = $y / ($h - 1.0) # 0.0 at top (nozzle), 1.0 at bottom (tail)
        
        # Vertical attenuation (smooth exponential fade)
        $alphaV = [math]::Pow(1.0 - $v, 1.3)
        if ($v -lt 0.05) {
            # Slight soft ramp-in at very nozzle edge
            $alphaV *= ($v / 0.05) * 0.3 + 0.7
        }

        # Color progression: white-hot -> cyan -> deep blue -> violet
        $r = 0.0
        $g = 0.0
        $b = 0.0
        if ($v -lt 0.15) {
            $t = $v / 0.15
            $r = 1.0 * (1.0 - $t) + 0.65 * $t
            $g = 1.0 * (1.0 - $t) + 0.90 * $t
            $b = 1.0
        } elseif ($v -lt 0.50) {
            $t = ($v - 0.15) / 0.35
            $r = 0.65 * (1.0 - $t) + 0.20 * $t
            $g = 0.90 * (1.0 - $t) + 0.60 * $t
            $b = 1.0
        } else {
            $t = ($v - 0.50) / 0.50
            $r = 0.20 * (1.0 - $t) + 0.10 * $t
            $g = 0.60 * (1.0 - $t) + 0.30 * $t
            $b = 1.0 * (1.0 - $t) + 0.80 * $t
        }

        for ($x = 0; $x -lt $w; $x++) {
            # Horizontal softness: slightly brighter along core
            $u = ($x / ($w - 1.0)) * 2.0 - 1.0 # -1 to +1
            $uFactor = 1.0 - 0.25 * ($u * $u)
            if ($uFactor -lt 0.0) { $uFactor = 0.0 }

            $finalA = [math]::Min(255, [math]::Max(0, [int]($alphaV * $uFactor * 255.0)))
            $finalR = [math]::Min(255, [math]::Max(0, [int]($r * 255.0)))
            $finalG = [math]::Min(255, [math]::Max(0, [int]($g * 255.0)))
            $finalB = [math]::Min(255, [math]::Max(0, [int]($b * 255.0)))

            $color = [System.Drawing.Color]::FromArgb($finalA, $finalR, $finalG, $finalB)
            $bmp.SetPixel($x, $y, $color)
        }
    }

    $outPath = "assets/textures/afterburner_flame.png"
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Saved $outPath"
}

function CreateShockDiamondTexture {
    $w = 256
    $h = 256
    $bmp = New-Object System.Drawing.Bitmap($w, $h)

    for ($y = 0; $y -lt $h; $y++) {
        $ny = ($y / ($h - 1.0)) * 2.0 - 1.0 # -1 to 1
        for ($x = 0; $x -lt $w; $x++) {
            $nx = ($x / ($w - 1.0)) * 2.0 - 1.0 # -1 to 1

            # Diamond distance: |x| + |y| (Manhattan distance creates sharp diamond)
            $dDiamond = [math]::Abs($nx) * 1.3 + [math]::Abs($ny) * 0.85
            $dCircle = [math]::Sqrt($nx * $nx + $ny * $ny)
            $dist = $dDiamond * 0.65 + $dCircle * 0.35

            if ($dist -ge 1.0) {
                $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
                continue
            }

            $falloff = [math]::Pow(1.0 - $dist, 1.8)
            $core = [math]::Pow([math]::Max(0.0, 1.0 - $dist * 2.5), 2.0)

            $r = [math]::Min(255, [int]((0.35 + 0.65 * $core) * 255.0))
            $g = [math]::Min(255, [int]((0.75 + 0.25 * $core) * 255.0))
            $b = 255
            $a = [math]::Min(255, [int]($falloff * 255.0))

            $color = [System.Drawing.Color]::FromArgb($a, $r, $g, $b)
            $bmp.SetPixel($x, $y, $color)
        }
    }

    $outPath = "assets/textures/shock_diamond.png"
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Saved $outPath"
}

CreateFlameTexture
CreateShockDiamondTexture
