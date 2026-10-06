# Calculate world positions of m21.gltf nose and tail under rotation {0, pi, 0}
$pi = [math]::PI
$noseLocal = @(0.0, 0.0, -0.25)
$tailLocal = @(0.0, 0.0, 15.0)

# Under Y rotation by pi: (x, y, z) -> (-x, y, -z)
$noseWorld = @(-$noseLocal[0], $noseLocal[1], -$noseLocal[2])
$tailWorld = @(-$tailLocal[0], $tailLocal[1], -$tailLocal[2])

Write-Output "With rot = {0, pi, 0}:"
Write-Output "Nose world Z = $($noseWorld[2])"
Write-Output "Tail (nozzle) world Z = $($tailWorld[2])"

# Under rot = {0, 0, 0}:
Write-Output "`nWith rot = {0, 0, 0}:"
Write-Output "Nose world Z = $($noseLocal[2])"
Write-Output "Tail (nozzle) world Z = $($tailLocal[2])"
