# Runtime snapshot bridge

Specified before the runtime snapshot reader. The M1 raw configuration catalog remains provisional. A runtime observation is a separate document captured from a loaded game and save; a comparison against the installed game and Principia prediction is a later, stronger claim.

## Capture boundary

The document contains a schema version, exporter identity and version, KSP version, loaded mod identities, save identity, capture UT in SI seconds, the source of the state's gravitational parameters, and one state for every massive body at that same UT. Every body has a stable ID, parent ID (except the root), positive gravitational parameter and radius, optional nonnegative atmosphere height, and finite Cartesian position and velocity. Position is metres and velocity is metres per second. A canonical source file hash is computed from the exact UTF-8 document bytes when read. Duplicate IDs, hierarchy cycles, missing parents, mismatched epochs, nonfinite values, overlap and missing provenance fail the reader.

The exported frame records an origin, axes, handedness, inertial classification and the source-to-export transform. A source frame called `World` is insufficient. [Principia's frame definitions](https://github.com/mockingbirdnest/Principia/blob/master/ksp_plugin/frames.hpp) call `World` arbitrary and left handed; `Barycentric` is inertial and right handed. [Its `CelestialFromParent` implementation](https://github.com/mockingbirdnest/Principia/blob/master/ksp_plugin/plugin.cpp) returns parent-relative states rotated into right handed `AliceSun` coordinates at the current instant. A valid initial-state capture may freeze that instantaneous basis, accumulate parent-relative vectors to the root, and subtract the mass-weighted centre of mass. The exporter must record that procedure and the source version. Repeated samples in a changing `AliceSun` basis are not a time series in one fixed inertial frame.

The reader accepts `runtime_observed_uncompared` only with a complete source and transform record. It does not infer that a C# field exists in the installed version or claim the export has occurred. `runtime_verified` additionally requires a separate comparison record identifying the installed game/Principia version, compared UTs, bodies, force settings and measured position/velocity error bounds. The independent M2 Newtonian mode may use an observed initial state after the frame gate, but its result label remains `independent_newtonian_nbody`. Principia-matched mode remains unavailable until its model has independent comparison evidence.

The calendar display is a separate 365-day no-leap labeling of UT with explicit day duration and display origin. Game UT zero and Kronometer settings are recorded separately; neither is silently treated as the display origin.

## Reader failure cases

| Input | Observable result |
|---|---|
| M1 raw catalog or missing runtime exporter, game, mod or save identity | Reject as a numerical runtime snapshot. |
| Missing state epoch, mixed body epochs, missing/unknown frame, left-handed or rotating output frame, or unrecorded transform | Reject before integration. |
| Duplicate body ID, missing parent, cycle, multiple roots, nonpositive/nonfinite radius or gravitational parameter, nonfinite state, overlapping bodies | Reject with a body-specific diagnostic. |
| Missing or inconsistent Principia condition, or a claim of Principia equivalence without comparison evidence | Reject the claim. |
| Missing or invalid no-leap display origin, day duration or month lengths | Reject calendar conversion. |
| Changed source file bytes | Produce a different SHA-256 cache identity; the C++ runtime bridge recomputes SHA-256 and rejects a supplied hash that does not match the exact JSON bytes. |

Successful JSON parsing is only a schema check. No real JNSQ runtime state is available yet, and neither this contract nor synthetic reader tests promote the M1 catalog.
