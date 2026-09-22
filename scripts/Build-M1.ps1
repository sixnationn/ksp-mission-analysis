$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    dotnet run --project tests/KspMission.Import.Tests/KspMission.Import.Tests.csproj --configuration Release
    if ($LASTEXITCODE -ne 0) { throw 'M1 tests failed' }

    dotnet publish src/KspMission.Import/KspMission.Import.csproj --configuration Release `
        -p:UseAppHost=true --output artifacts/m1/windows
    if ($LASTEXITCODE -ne 0) { throw 'M1 Windows publish failed' }

    Write-Output "Windows importer: $(Join-Path $root 'artifacts/m1/windows/KspMission.Import.exe')"
}
finally {
    Pop-Location
}
