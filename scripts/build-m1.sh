#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

dotnet run --project tests/KspMission.Import.Tests/KspMission.Import.Tests.csproj --configuration Release
dotnet publish src/KspMission.Import/KspMission.Import.csproj --configuration Release \
  -p:UseAppHost=false --output artifacts/m1/ubuntu-portable

printf 'Ubuntu portable importer: %s\n' "$root/artifacts/m1/ubuntu-portable/KspMission.Import.dll"
