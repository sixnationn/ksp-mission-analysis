param(
    [string]$InputDirectory = '.work/inputs',
    [string]$RuntimeGameDirectory = '.work/runtime/ksp',
    [string[]]$OnlyArchives = @()
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression

$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$inputs = [IO.Path]::GetFullPath((Join-Path $workspace $InputDirectory))
$game = [IO.Path]::GetFullPath((Join-Path $workspace $RuntimeGameDirectory))
$work = [IO.Path]::GetFullPath((Join-Path $workspace '.work'))
if (-not $game.StartsWith($work + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Runtime directory must remain inside workspace .work'
}
if (-not (Test-Path -LiteralPath (Join-Path $game 'KSP_x64.exe'))) {
    throw 'Expected disposable KSP extraction at the runtime directory'
}

$archives = @(
    'HarmonyKSP_2.2.1.0_for_KSP1.8+.zip',
    'ModularFlightIntegrator-1.2.10.0.zip',
    'KSPTextureLoader.zip',
    'KSPBurst_1.7.4.11.zip',
    'KSPBurst_Compiler_1.7.4.11.zip',
    'Kopernicus-1.12.1-247.zip',
    'VertexMitchellNetravaliMap.zip',
    'ParallaxContinued-1.0.4.zip',
    'ParallaxContinued_StockTerrainTextures-1.0.3.zip',
    'ParallaxContinued_StockPlanetTextures-1.0.3.zip',
    'ParallaxContinued_StockScatterTextures-1.0.3.zip',
    'Deferred_1.3.5.0.zip',
    'RaymarchedVolumetricsEarlyAccess23_08_26.zip',
    'Kronometer-1.12.0-1.12.0.2.zip',
    'JNSQ.0.10.2.zip',
    'JNSQ-Reborn-v1.0.1.zip',
    'principia-levy-1.12.5.zip',
    'ModuleManager-4.2.3.zip'
)
$manifestPath = Join-Path $game 'ksp-mission-runtime-manifest.json'
$manifest = @()
if ((Test-Path -LiteralPath $manifestPath) -and $OnlyArchives.Count -gt 0) {
    $manifest = @(Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json)
}
foreach ($name in $archives) {
    if ($OnlyArchives.Count -gt 0 -and $name -notin $OnlyArchives) { continue }
    $path = Join-Path $inputs $name
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing approved runtime archive: $name" }
    $zip = [IO.Compression.ZipFile]::OpenRead($path)
    $entries = @()
    try {
        foreach ($entry in $zip.Entries) {
            $relative = $entry.FullName.Replace('/', [IO.Path]::DirectorySeparatorChar)
            if ($name -eq 'ModuleManager-4.2.3.zip') {
                if ($entry.Name -ne 'ModuleManager.4.2.3.dll') { continue }
                $relative = Join-Path 'GameData' $entry.Name
            } elseif ($name -eq 'ModularFlightIntegrator-1.2.10.0.zip' -or $name -eq 'VertexMitchellNetravaliMap.zip') {
                if ($name -eq 'ModularFlightIntegrator-1.2.10.0.zip' -and
                    -not $entry.FullName.StartsWith('ModularFlightIntegrator/', [StringComparison]::Ordinal)) { continue }
                if ($name -eq 'VertexMitchellNetravaliMap.zip' -and
                    -not $entry.FullName.StartsWith('000_NiakoUtils/', [StringComparison]::Ordinal)) { continue }
                $relative = Join-Path 'GameData' $relative
            } elseif (-not $entry.FullName.StartsWith('GameData/', [StringComparison]::Ordinal)) {
                continue
            }
            $destination = [IO.Path]::GetFullPath((Join-Path $game $relative))
            if (-not $destination.StartsWith($game + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Unsafe archive path: $name :: $($entry.FullName)"
            }
            $entries += @{ Entry = $entry; Destination = $destination }
        }
        foreach ($item in $entries) {
            if (-not $item.Entry.Name) { continue }
            if (Test-Path -LiteralPath $item.Destination) {
                throw "Refusing to overwrite existing game file: $($item.Destination)"
            }
        }
        foreach ($item in $entries) {
            if (-not $item.Entry.Name) {
                [IO.Directory]::CreateDirectory($item.Destination) | Out-Null
                continue
            }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.Destination)) | Out-Null
            $source = $item.Entry.Open()
            $target = [IO.File]::Open($item.Destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
            try { $source.CopyTo($target) } finally { $target.Dispose(); $source.Dispose() }
        }
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        $manifest += [pscustomobject]@{ file = $name; sha256 = $hash; installed_file_count = @($entries | Where-Object { $_.Entry.Name }).Count }
        Write-Output "$name installed_file_count=$(@($entries | Where-Object { $_.Entry.Name }).Count) sha256=$hash"
    } finally { $zip.Dispose() }
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
