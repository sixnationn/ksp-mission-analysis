# M2 persisted ephemeris cache contract

Written before implementation on 23 September 2026. This closes the in-memory cache gap in `M2-VALIDATION.md` for repeatable bounded mission runs. It does not turn an independent Newtonian trajectory into a Principia result.

## Snapshot and file contract

The cache is an immutable, versioned binary artifact. Save and load both require the complete `Snapshot` and `Settings` used for integration. The file contains a magic and format version, all ephemeris metadata, body descriptors in snapshot order (stable ID, gravitational parameter in m^3/s^2, radius in m, Cartesian initial state in m and m/s, state epoch in UT seconds), and every boundary state in body-major order. Numeric fields use IEEE-754 binary64 and fixed little-endian encoding. A trailing SHA-256 of preceding bytes detects accidental damage. No path, machine-specific timestamp, or installed-KSP claim belongs in the artifact.

Load accepts only a file whose exact source hash and confidence, frame and epoch, body count/order/parameters/initial states, integrator and interpolation identities, coverage, step, fit limits, and measured fit errors agree with the caller. It validates finite values, sample dimensions, first samples, supported version, exact file length, checksum, and a bounded file/sample count before constructing a queryable ephemeris. `max_steps` is an admission limit on reload. A saved `synthetic_fixture` remains synthetic; a runtime-derived snapshot retains its precise runtime confidence. Stored fit errors document the original independent sixfold-finer comparison and are not remeasured by loading. SHA-256 here is an integrity check, not authentication or proof that the source/runtime states are correct.

Saving writes to a unique temporary file in the destination directory and publishes the completed artifact without replacing an existing destination. A failed save or load leaves any previous cache untouched. A caller can choose a new cache path for changed inputs. The cache format has an explicit version; incompatible versions fail rather than being silently interpreted.

## Observable failure cases

| Case | Required result |
|---|---|
| Round trip of a complete synthetic two-body ephemeris | Same metadata/body order/sample states and off-grid query values; both coverage endpoints work. |
| Reuse with changed snapshot hash, source confidence, frame, epoch, body order, mu, radius, initial state, step, coverage, fit limit, or `max_steps` below stored count | Reject before exposing samples. Reusing the same hash with changed physics is still a miss. |
| Provisional, incomplete, non-inertial, or nonfinite input snapshot; unsupported result label | Reject save/load even if a crafted file claims compatibility. |
| Wrong magic/version, truncated or appended file, changed payload/checksum, count overflow, overlarge file, duplicate/empty body ID, nonfinite metadata or sample | Explicit failure, no partially queryable result. |
| Existing destination or invalid cache on save | Existing file remains byte-for-byte unchanged; no partial destination. |
| Query outside loaded coverage or for missing body | Existing `Ephemeris::query` error, no extrapolation/fallback. |

The tests use a short synthetic integration, mutate both caller inputs and file bytes, and check that rejected files never become queryable. CI must pass on Ubuntu and Windows. No installed KSP/Principia comparison is implied.
