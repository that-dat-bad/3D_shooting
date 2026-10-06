Add-Type -AssemblyName System.Drawing
$srcPath = 'C:\Users\gamer\.gemini\antigravity-ide\brain\b2ac1d53-83e0-439c-9ec2-fd7967f70d6e\realistic_smoke_puff_1791307588888.jpg'
$destPath = 'assets\textures\smoke_puff.png'

$src = [System.Drawing.Bitmap]::FromFile($srcPath)
$dest = New-Object System.Drawing.Bitmap($src.Width, $src.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

$rect = New-Object System.Drawing.Rectangle(0, 0, $src.Width, $src.Height)
$srcData = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$destData = $dest.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

$srcStride = $srcData.Stride
$destStride = $destData.Stride
$srcBuf = New-Object byte[] ($srcStride * $src.Height)
$destBuf = New-Object byte[] ($destStride * $dest.Height)

[System.Runtime.InteropServices.Marshal]::Copy($srcData.Scan0, $srcBuf, 0, $srcBuf.Length)

for ($y = 0; $y -lt $src.Height; $y++) {
    for ($x = 0; $x -lt $src.Width; $x++) {
        $sIdx = $y * $srcStride + $x * 3
        $b = $srcBuf[$sIdx]
        $g = $srcBuf[$sIdx + 1]
        $r = $srcBuf[$sIdx + 2]
        
        $lum = [Math]::Max($r, [Math]::Max($g, $b))
        
        # Soft curve
        $a = 0
        if ($lum -gt 6) {
            $norm = [Math]::Min(1.0, ($lum - 6) / 160.0)
            $a = [int]($norm * 255.0)
        }
        
        $dIdx = $y * $destStride + $x * 4
        # RGB is white/neutral so ParticleManager color tints it properly
        $destBuf[$dIdx] = [byte]255     # B
        $destBuf[$dIdx + 1] = [byte]255 # G
        $destBuf[$dIdx + 2] = [byte]255 # R
        $destBuf[$dIdx + 3] = [byte]$a   # A
    }
}

[System.Runtime.InteropServices.Marshal]::Copy($destBuf, 0, $destData.Scan0, $destBuf.Length)
$src.UnlockBits($srcData)
$dest.UnlockBits($destData)

if (Test-Path $destPath) {
    Copy-Item $destPath 'assets\textures\smoke_puff_old.png' -Force
}
$dest.Save($destPath, [System.Drawing.Imaging.ImageFormat]::Png)
$src.Dispose()
$dest.Dispose()
Write-Host 'SUCCESS: Realistic smoke_puff.png created!'
