$json = Get-Content 'Resources/models/m21_wg.gltf' -Raw | ConvertFrom-Json
$bin = [System.IO.File]::ReadAllBytes('Resources/models/m21_wg.bin')

# Node 13 is Fuse.001, mesh 13
$m = $json.meshes[13]
$prim = $m.primitives[0]
$posAccIdx = $prim.attributes.POSITION
$acc = $json.accessors[$posAccIdx]
$bv = $json.bufferViews[$acc.bufferView]

$offset = 0
if ($bv.byteOffset -ne $null) { $offset += $bv.byteOffset }
if ($acc.byteOffset -ne $null) { $offset += $acc.byteOffset }

$count = $acc.count
$tailVerts = [System.Collections.Generic.List[object]]::new()

for ($i = 0; $i -lt $count; $i++) {
    $idx = $offset + $i * 12
    $x = [System.BitConverter]::ToSingle($bin, $idx)
    $y = [System.BitConverter]::ToSingle($bin, $idx + 4)
    $z = [System.BitConverter]::ToSingle($bin, $idx + 8)
    if ($z -gt 6.8) {
        $tailVerts.Add([PSCustomObject]@{ X = [math]::Round($x, 3); Y = [math]::Round($y, 3); Z = [math]::Round($z, 3) })
    }
}

Write-Output "Found $($tailVerts.Count) verts with Z > 6.8"
$left = $tailVerts | Where-Object { $_.X -lt -0.1 }
$right = $tailVerts | Where-Object { $_.X -gt 0.1 }

if ($left.Count -gt 0) {
    $avgLx = ($left | Measure-Object -Property X -Average).Average
    $avgLy = ($left | Measure-Object -Property Y -Average).Average
    $maxLz = ($left | Measure-Object -Property Z -Maximum).Maximum
    Write-Output "Left Nozzle: AvgX=$([math]::Round($avgLx, 3)), AvgY=$([math]::Round($avgLy, 3)), MaxZ=$([math]::Round($maxLz, 3)), Verts=$($left.Count)"
}
if ($right.Count -gt 0) {
    $avgRx = ($right | Measure-Object -Property X -Average).Average
    $avgRy = ($right | Measure-Object -Property Y -Average).Average
    $maxRz = ($right | Measure-Object -Property Z -Maximum).Maximum
    Write-Output "Right Nozzle: AvgX=$([math]::Round($avgRx, 3)), AvgY=$([math]::Round($avgRy, 3)), MaxZ=$([math]::Round($maxRz, 3)), Verts=$($right.Count)"
}

# Also show min and max X, Y of these tail verts
$minLx = ($left | Measure-Object -Property X -Minimum).Minimum
$maxLx = ($left | Measure-Object -Property X -Maximum).Maximum
$minLy = ($left | Measure-Object -Property Y -Minimum).Minimum
$maxLy = ($left | Measure-Object -Property Y -Maximum).Maximum
Write-Output "Left Nozzle Bounds: X=[$minLx, $maxLx], Y=[$minLy, $maxLy]"
