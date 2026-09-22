param([string]$RuntimeGameDirectory = '.work/runtime/ksp')

$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$game = [IO.Path]::GetFullPath((Join-Path $workspace $RuntimeGameDirectory))
$work = [IO.Path]::GetFullPath((Join-Path $workspace '.work'))
if (-not $game.StartsWith($work + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Runtime directory must remain inside workspace .work'
}
$config = Join-Path $game 'GameData/JNSQ-Reborn/JNSQReborn-Configuration.cfg'
if (-not (Test-Path -LiteralPath $config)) { throw 'JNSQ Reborn runtime config missing' }
$content = [IO.File]::ReadAllText($config)
$old = "`tSystemScale = Standard"
$new = "`tSystemScale = Real"
if ($content.Contains($old)) {
    $backup = Join-Path $game 'ksp-mission-original-JNSQReborn-Configuration.cfg'
    if (Test-Path -LiteralPath $backup) { throw 'Original config backup already exists' }
    [IO.File]::WriteAllText($backup, $content)
    $content = $content.Replace($old, $new)
    [IO.File]::WriteAllText($config, $content)
} elseif (-not $content.Contains($new)) {
    throw 'Unrecognized Reborn SystemScale setting'
}
if ($content.Contains($old)) { throw 'Reborn Standard scale remains selected' }
if (-not $content.Contains('RealTime = True')) { throw 'Reborn RealTime selection changed unexpectedly' }
"Reborn Real scale config SHA-256: $((Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash)"
