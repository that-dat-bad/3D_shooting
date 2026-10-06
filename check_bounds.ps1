$json = Get-Content 'Resources/models/m21_wg.gltf' -Raw | ConvertFrom-Json
for ($i=0; $i -lt $json.nodes.Count; $i++) {
    $n = $json.nodes[$i]
    $mIdx = $n.mesh
    if ($mIdx -ne $null) {
        $m = $json.meshes[$mIdx]
        $accIdx = $m.primitives[0].attributes.POSITION
        $acc = $json.accessors[$accIdx]
        $minStr = [string]::Join(", ", ($acc.min | ForEach-Object { [math]::Round($_, 3) }))
        $maxStr = [string]::Join(", ", ($acc.max | ForEach-Object { [math]::Round($_, 3) }))
        Write-Output "Node $i ($($n.name)) -> Mesh $mIdx ($($m.name)) | MIN: [$minStr] | MAX: [$maxStr]"
    }
}
