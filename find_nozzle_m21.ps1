$json = Get-Content 'Resources/models/m21.gltf' -Raw | ConvertFrom-Json
$bin = [System.IO.File]::ReadAllBytes('Resources/models/m21.bin')

# Find mesh for Fuse
$m = $json.meshes[9] # Fuse
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
    if ($z -gt 14.0) {
        $tailVerts.Add([PSCustomObject]@{ X = [math]::Round($x, 3); Y = [math]::Round($y, 3); Z = [math]::Round($z, 3) })
    }
}

$left = $tailVerts | Where-Object { $_.X -lt -0.1 }
$right = $tailVerts | Where-Object { $_.X -gt 0.1 }

$avgLx = ($left | Measure-Object -Property X -Average).Average
$avgLy = ($left | Measure-Object -Property Y -Average).Average
$maxLz = ($left | Measure-Object -Property Z -Maximum).Maximum
Write-Output "m21.gltf Left Nozzle: AvgX=$([math]::Round($avgLx, 3)), AvgY=$([math]::Round($avgLy, 3)), MaxZ=$([math]::Round($maxLz, 3))"

$avgRx = ($right | Measure-Object -Property X -Average).Average
$avgRy = ($right | Measure-Object -Property Y -Average).Average
$maxRz = ($right | Measure-Object -Property Z -Maximum).Maximum
Write-Output "m21.gltf Right Nozzle: AvgX=$([math]::Round($avgRx, 3)), AvgY=$([math]::Round($avgRy, 3)), MaxZ=$([math]::Round($maxRz, 3))"
