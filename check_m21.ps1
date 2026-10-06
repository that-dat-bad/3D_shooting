$json = Get-Content 'Resources/models/m21.gltf' -Raw | ConvertFrom-Json
$bin = [System.IO.File]::ReadAllBytes('Resources/models/m21.bin')
foreach ($n in $json.nodes) {
    if ($n.mesh -ne $null) {
        $m = $json.meshes[$n.mesh]
        $acc = $json.accessors[$m.primitives[0].attributes.POSITION]
        $minStr = [string]::Join(", ", ($acc.min | ForEach-Object { [math]::Round($_, 3) }))
        $maxStr = [string]::Join(", ", ($acc.max | ForEach-Object { [math]::Round($_, 3) }))
        Write-Output "Node $($n.name) | MIN: [$minStr] | MAX: [$maxStr]"
    }
}
