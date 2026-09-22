# KSP mission analysis

An external mission-analysis tool for Kerbal Space Program, currently at the **M1 system-import milestone**. Ubuntu/Linux is the primary target; a Windows build is also required. The optimizer and desktop UI have not been started.

The current command-line importer reads a bounded set of JNSQ Reborn Real configuration fields, selects the Principia on/off branch, and emits a provenance-bearing JSON catalog. This is a **provisional raw-config preview**, not a verified loaded KSP/Principia state or an analysis-ready ephemeris. It must not be used to claim Principia-equivalent trajectories.

## Build and check

Requires an installed .NET 10 SDK. No additional development packages are needed for M1.

On Ubuntu:

```bash
./scripts/build-m1.sh
dotnet artifacts/m1/ubuntu-portable/KspMission.Import.dll --reborn <extracted-reborn-config-dir> --jnsq <extracted-jnsq-config-dir> --scale Real --principia on
```

On Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build-M1.ps1
artifacts/m1/windows/KspMission.Import.exe --reborn <extracted-reborn-config-dir> --jnsq <extracted-jnsq-config-dir> --scale Real --principia on
```

Use `--principia off` to inspect the other branch. The optional `--reborn-archive <ZIP> --jnsq-archive <ZIP>` arguments record the source archive hashes. The importer writes JSON to standard output; redirect it to a file if needed. An unmodified Reborn v1.0.1 release defaults to Standard scale, so a Real-scale preview reports a scale-selection mismatch unless the source configuration is set to Real. Do not change an existing KSP installation to do this.

The Windows checks and publish passed at M1. The portable Linux artifact was published on Windows; **native Ubuntu execution remains unverified**. See [M1 validation](docs/M1-VALIDATION.md) for exact evidence and gaps.

## Project map

- [Product plan and milestones](docs/PLAN.md)
- [Input versions and provenance](docs/M0-INPUT-REPORT.md)
- [Snapshot contract and failure cases](docs/SNAPSHOT-CONTRACT.md)
- [Importer source](src/KspMission.Import/) and [focused checks](tests/KspMission.Import.Tests/)
- [M1 showcase and graphs](reports/m1-showcase/index.html)
- [Token benchmark method](benchmarks/README.md)

Game archives, extracted mods, local browser data, and generated binaries stay under ignored `.work/`, `artifacts/`, or build output directories. They are not included in this repository. Benchmark session paths refer to local Codex logs and are useful only on the original development machine; the committed milestone snapshots and reports are the portable record.
