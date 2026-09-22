param([string]$RuntimeGameDirectory = '.work/runtime/ksp')

$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$game = [IO.Path]::GetFullPath((Join-Path $workspace $RuntimeGameDirectory))
$work = [IO.Path]::GetFullPath((Join-Path $workspace '.work'))
if (-not $game.StartsWith($work + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Runtime directory must remain inside workspace .work'
}
$settings = Join-Path $game 'settings.cfg'
if (-not (Test-Path -LiteralPath $settings)) { throw 'Disposable KSP settings.cfg missing' }
$content = [IO.File]::ReadAllText($settings)
foreach ($key in @('MASTER_VOLUME','SHIP_VOLUME','AMBIENCE_VOLUME','MUSIC_VOLUME','UI_VOLUME','VOICE_VOLUME')) {
    $pattern = '(?m)^(' + $key + '\s*=\s*)[^\r\n]+'
    if ([regex]::Matches($content, $pattern).Count -ne 1) { throw "Expected one $key setting" }
    $content = [regex]::Replace($content, $pattern, '${1}0')
}
[IO.File]::WriteAllText($settings, $content)
"Muted all six disposable KSP volume settings in $settings"
