# M1 import validation

Run on 22 September 2026 from the Windows development host. M1 implements a bounded raw-config importer for JNSQ Reborn Real, with explicit Principia on/off selection. It is a provisional catalog, not a resolved KSP or Principia state. The schema and failure cases were written in `SNAPSHOT-CONTRACT.md` before implementation.

## Input and scenario

The pinned JNSQ 0.10.2 and Reborn v1.0.1 release ZIP hashes are in `M0-INPUT-REPORT.md`. The Reborn release config defaults to `SystemScale=Standard`. An untouched-release import therefore reports `ScaleSelection=requested_override_preview` when Real is requested. For the primary Real comparison, the exact extracted Reborn config tree was copied to ignored `.work/scenarios/JNSQ-Reborn-Real/` and its one `SystemScale = Standard` assignment changed to `SystemScale = Real`. No game installation was changed. The modified `JNSQReborn-Configuration.cfg` SHA-256 is `e4cdf9478b5325c2995d2f0d625adeb269cedb2957033e2a4fb77e750fe64f12`; the catalog includes its file hash alongside the original ZIP hash. It reports `SourceConfiguredScale=Real` and `ScaleSelection=matches_source`.

## Checks

`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build-M1.ps1` passed 24/24 focused checks and published `artifacts/m1/windows/KspMission.Import.exe`. The checks cover double scaling, required Principia choice, branch selection, Real patching, unresolved patches, duplicate/missing/cyclic body hierarchy, invalid radius and gravitational parameter, field provenance, zero atmosphere boundary, provisional confidence, no-leap calendar boundaries, anomaly units and ambiguity, source-scale mismatch, unresolved imported calendar, a missing Orbit node, and embedded optional 10X config. The blocking anomaly, scale-selection, calendar and hierarchy failures were represented in tests before their importer fixes were made.

`dotnet publish src/KspMission.Import/KspMission.Import.csproj --configuration Release -p:UseAppHost=false --output artifacts/m1/ubuntu-portable` succeeded with the installed .NET 10 SDK. The primary Ubuntu artifact is a framework-dependent, portable `KspMission.Import.dll`, run with `dotnet KspMission.Import.dll`. `scripts/build-m1.sh` runs the focused checks and creates that portable artifact on Ubuntu. Native Ubuntu execution and shell-script syntax execution were unavailable on this Windows host, so Linux runtime validation remains open. The Windows executable was used for the release-scenario imports below. No new development dependency was installed.

Both Principia branches were imported from the isolated Real scenario with the published Windows executable and both original release ZIPs named for provenance. Each produced 32 unique bodies, one root, 106 hashed source entries, 12 diagnostics, `Confidence=raw_config_provisional`, and `AnalysisReady=false`. All 31 nonroot orbit element epochs are explicitly 0 UT seconds and all selected mean anomalies are present. The two JSON results are in ignored `artifacts/m1/configured-real-principia-{on,off}.json`.

| Source-derived field | Principia on | Principia off |
|---|---:|---:|
| Kerbin radius | 6,400,000 m | 6,400,000 m |
| Kerbin rotation period | 86,400 s | 86,400 s |
| Mun semi-major axis | 375,360,000 m | 363,840,000 m |
| Minmus semi-major axis | 234,200,000 m | 587,880,000 m |
| Bop mean anomaly | 90 degrees | 270 degrees |
| Duna mean anomaly | 51.5662015617741 degrees, from 0.9 radians | same |

The requested tool display calendar uses a 86,400 SI-second day, 365 days and no leap years, with display origin UT 0. The imported Kronometer preview separately records `RealTime=True`, 12 source month lengths, offset year 2001, offset day 1 and Real patch offset time 43,200 s. Its `Resolved` flag is false; game UT zero, loaded home-day duration and `useLeapYears` remain unknown. The tool calendar does not establish a KSP epoch.

## Remaining gaps at the M1 boundary

- There is no resolved ModuleManager cache, loaded JNSQ/Principia save, Cartesian body state, common state epoch, validated frame, or verified force model. No trajectories or Principia-equivalence claim can use this catalog as a ready numerical snapshot.
- The importer selects a bounded subset of raw fields and known Real rescale patches. Other ModuleManager conditions and physical/calendar patches remain diagnostics; a complete in-game resolution needs a controlled modded KSP load and export.
- Stock, JNSQ Standard, JNSQ optional 10X and Reborn Standard have not been compared as full fixture matrices. The M1 validated fixture is Reborn Real with Principia on/off.
- Ubuntu native build/run, packaging, and KSP runtime comparison remain unverified. Windows publish and executable imports passed; the portable .NET artifact is Linux compatible by design but was built on Windows.

M1 stops here. The optimizer and UI have not been started.
