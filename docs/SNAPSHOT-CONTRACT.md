# M1 system snapshot contract and failure cases

Established before M1 importer code on 22 September 2026. This is the boundary consumed by later numerical work. A config preview is a catalog, not a usable trajectory snapshot.

## Inputs and confidence

The first adapter targets JNSQ 0.10.2 plus JNSQ Reborn v1.0.1, with `SystemScale=Real`, `RealTime=True`, and Principia presence stated explicitly. Reborn Real and JNSQ optional `Rescale_10X` are mutually exclusive choices in this adapter. No partial ModuleManager interpreter may claim to reproduce an arbitrary GameData load.

Every output records its input file names, SHA-256 hashes, source/version, chosen scale, Principia condition, other known mod conditions, resolution method, and diagnostics. Confidence is one of:

The release ZIP itself has `SystemScale=Standard`. A requested Real interpretation of that untouched file is labeled `requested_override_preview` and records the source setting. An isolated copy with `SystemScale=Real` records `matches_source` and the modified file hash. Neither label implies a ModuleManager resolution or a KSP load.

- `raw_config_provisional`: selected source configs and known branch rules only; unhandled patches are listed.
- `resolved_config_baseline`: a complete, identified ModuleManager cache plus config inputs; it describes a new-system baseline at a stated epoch, pending runtime comparison.
- `runtime_verified`: exported from the loaded KSP/Principia installation and save, with exporter version, game/mod versions, save identity and comparison evidence.

Do not promote confidence from one level to another because the numbers look plausible. A Principia save can evolve away from its initial configs.

## Canonical data

The versioned snapshot has stable internal body IDs distinct from display names, one parent ID per non-root body, and an acyclic hierarchy. For each body it stores gravitational parameter (`m^3/s^2`), equatorial reference radius (`m`), atmosphere boundary above the reference radius (`m`, or explicit none), rotation state and orientation convention, applicable gravity harmonics with their normalization, and position (`m`) and velocity (`m/s`) at the same explicit initial-state epoch.

The state frame has a named origin, axis basis, handedness and inertial/rotating classification. A coordinate transform, when applied, is recorded with its version and source. Do not convert Principia's Jacobi fallback elements as parent-relative two-body elements. If the source supplies only orbital elements, retain their coordinate convention and raw values as evidence; Cartesian state remains absent until a validated conversion or runtime export exists.

Time fields are separate: `state_epoch_ut_s` (the instant of Cartesian states), `game_ut_zero` (KSP's simulation origin), and `display_origin` (calendar labeling). Internal time is SI seconds. The requested tool calendar is a 365-day, no-leap calendar with month lengths `[31,28,31,30,31,30,31,31,30,31,30,31]`; its day duration and origin must be explicit. `Y0,D0` means year and ordinal day zero at the chosen display origin. Imported Kronometer settings and offsets are stored separately and never silently substituted for the tool display origin.

Orbit source elements retain their own epoch and per-field source keys, units and branch conditions. In Kopernicus configs, `meanAnomalyAtEpoch` is radians and `meanAnomalyAtEpochD` is degrees; a degree-valued preview converts the former explicitly. An element `epoch=0` is retained as an element epoch, not promoted to a common Cartesian state epoch.

Solver, force model and integration settings are metadata even at M1. A later independent Newtonian n-body solver must be identified as such. `analysis_ready` is true only if the complete body states, units, epoch, frame, force model and required gravity data pass validation. Provisional catalogs always set it false and explain why.

## Observable failures to test before implementation

| Input or condition | Required observation |
|---|---|
| Reborn Real and JNSQ optional 10X both selected | Reject double scaling; no body table marked resolved. |
| Principia flag absent or inconsistent with a conditional file | Reject the branch selection. Reborn Mun and JNSQ Minmus must select their distinct Principia on/off axes. |
| Unknown ModuleManager patch affecting a physical or calendar field | List the file/node and keep the catalog provisional. |
| Duplicate body ID, missing parent, parent cycle, invalid radius or gravitational parameter | Reject the canonical snapshot with body-specific diagnostics. |
| A nonroot body has no Orbit node or the catalog has multiple roots | Reject the catalog instead of silently creating another root. |
| Radian and degree anomaly fields both active, or a selected anomaly branch mislabeled | Reject ambiguity; preserve the selected source key, unit and branch. |
| Missing epoch, position, velocity, frame origin, axes or handedness | Keep `analysis_ready=false`; numerical consumers reject the snapshot. |
| Jacobi elements labeled parent-relative or converted by a two-body formula | Reject as a frame/coordinate-system error. |
| Non-finite or non-SI numeric values | Reject with field path and expected unit. |
| `useLeapYears=true`, absent day duration, bad month sum, or a missing display origin | Reject no-leap calendar conversion instead of guessing. |
| Calendar boundaries around day 0, day 365, negative UT, and 28 February to 1 March | Convert reproducibly with no leap day and exact SI-second boundaries. |
| A config-derived catalog presented as an old Principia save state | Reject confidence promotion; require runtime save export. |

## M1 acceptance evidence

Compare stock, JNSQ standard, JNSQ optional 10X, Reborn Standard and Reborn Real, each with Principia off/on where applicable. Record body count, stable IDs, hierarchy, radius, gravitational parameter, orbit and rotation, epoch, and calendar boundaries. The first working fixture is Reborn Real with Principia on; other cases remain explicit gaps until tested. Config-derived comparisons must say `provisional` unless an actual KSP load is observed. M1 stops before the planetary propagator, optimizer and UI.

The importer and snapshot format must run on Ubuntu/Linux and Windows. Windows-only validation on this development host is evidence for Windows only; a Linux-target publish is not a substitute for native Ubuntu execution.
