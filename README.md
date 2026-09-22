# KSP mission analysis

An external mission-analysis tool for Kerbal Space Program. Ubuntu/Linux is the primary target; Windows builds are checked too. The repository contains a provisional JNSQ Reborn Real importer, an independent planetary and spacecraft numerical core, bounded three-leg patched-conic screening, a runtime-snapshot exporter/reader contract, a headless worker, a versioned study report, and a GTK 3D window. Full multi-leg n-body refinement and installed-game comparison are still in progress.

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

The Windows checks and publish passed at M1. A later [GitHub Actions run](https://github.com/sixnationn/ksp-mission-analysis/actions/runs/35780328709) ran the M1 checks and published importer artifacts on Ubuntu 24.04 and Windows 2022. The supplied JNSQ setup has not been loaded and compared with KSP/Principia on Ubuntu. See [M1 validation](docs/M1-VALIDATION.md) for exact evidence and gaps.

The C++ M2 kernel can be built with CMake and a C++20 compiler:

```text
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
```

The numerical core accepts complete synthetic Cartesian snapshots in memory, and a C++ bridge validates the narrow Principia runtime JSON schema. The provisional JNSQ catalog cannot be integrated until a loaded state, frame and gravitational parameters are exported. The in-game exporter builds against the supplied KSP assemblies, but no Principia flight snapshot has been captured or compared. See [M2 validation](docs/M2-VALIDATION.md), [runtime snapshot contract](docs/RUNTIME-SNAPSHOT-CONTRACT.md), and [runtime status](docs/RUNTIME-EXPORTER-STATUS.md).

The `ksp_worker` executable accepts one versioned JSON-line request per process. Its `start_mission` command verifies exact runtime bytes, frame, epoch, body roles and atmosphere boundaries, integrates an independent Newtonian ephemeris, then screens the home/Mars-role/Venus-role/home sequence with a fixed 5,184,000-second parking stay. It ranks optimistic four-burn patched-conic routes under a 100,000-cell work cap. The GTK `ksp_desktop` target opens a synthetic four-body 3D study for visualization; after importing an exact runtime snapshot, it provides mission fields, worker progress/cancellation, ranked screened routes and a saved historical JSON study. Search is disabled for the synthetic display scene. Results are not Principia-equivalent or fully n-body-refined. See the [worker protocol](docs/M6-WORKER-PROTOCOL.md), [worker validation](docs/M6-WORKER-VALIDATION.md), [report contract](docs/M5-REPORT-CONTRACT.md), and [route contract](docs/M6-ROUTE-CONTRACT.md).

On Ubuntu 24.04, install the approved GTK build packages listed in [dependencies](docs/DEPENDENCIES.md), then run the CMake commands above; `ksp_desktop` and `ksp_worker` are built when gtkmm4 and libepoxy are available. On Windows, use MSYS2 UCRT64 with the same approved package list and CMake commands. [GitHub Actions run 35796817236](https://github.com/sixnationn/ksp-mission-analysis/actions/runs/35796817236) passed all six native Ubuntu and Windows jobs for the route worker and report library. Uploaded executables are build-check artifacts, not standalone installers. The latest GTK mission form and report-save action require the next CI run and rendered review.

## Project map

- [Product plan and milestones](docs/PLAN.md)
- [Input versions and provenance](docs/M0-INPUT-REPORT.md)
- [Snapshot contract and failure cases](docs/SNAPSHOT-CONTRACT.md)
- [Importer source](src/KspMission.Import/) and [focused checks](tests/KspMission.Import.Tests/)
- [M1 showcase and graphs](reports/m1-showcase/index.html)
- [M2 ephemeris contract and validation](docs/M2-VALIDATION.md)
- [M3 spacecraft checks](docs/M3-VALIDATION.md)
- [M4 screening and refinement limits](docs/M4-SEARCH-CONTRACT.md)
- [M4 runtime worker checkpoint](docs/M4-RUNTIME-WORKER-VALIDATION.md)
- [M6 three-leg route assembly contract](docs/M6-ROUTE-CONTRACT.md)
- [M6 bounded grid and route worker checkpoints](docs/M6-BOUNDED-SEARCH-VALIDATION.md) and [worker evidence](docs/M6-WORKER-VALIDATION.md)
- [M5 worker bridge and study report evidence](docs/M5-WORKER-BRIDGE-VALIDATION.md) and [report evidence](docs/M5-REPORT-VALIDATION.md)
- [M6 route-screening checkpoint and limits](docs/M6-VALIDATION.md)
- [M5 desktop workflow and rendered slice](docs/M5-DESKTOP-CONTRACT.md)
- [M4 checkpoint evidence](docs/M4-VALIDATION.md) and [M5 checkpoint evidence](docs/M5-VALIDATION.md)
- [Disposable game runtime dependency and load evidence](docs/GAME-RUNTIME-DEPENDENCIES.md)
- [Token benchmark method](benchmarks/README.md)

Game archives, extracted mods, local browser data, and generated binaries stay under ignored `.work/`, `artifacts/`, or build output directories. They are not included in this repository. Benchmark session paths refer to local Codex logs and are useful only on the original development machine; the committed milestone snapshots and reports are the portable record.
