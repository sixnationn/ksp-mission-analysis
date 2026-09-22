# M2 planetary ephemeris validation

Local Windows checks on 22 September 2026. The C++20 headless core integrates a complete synthetic Cartesian system in SI units using Newtonian point-mass gravity and velocity Verlet. It exposes a piecewise cubic Hermite position/velocity ephemeris with explicit coverage, identity and measured reference disagreement. Safety substeps are taken when a conservative displacement bound cannot clear a body pair; the method/version label records this. The integration is **independent Newtonian n-body**, not Principia-matched.

The contract and failure cases were written in `M2-EPHEMERIS-CONTRACT.md` before the builder implementation. A Sol High review found global drift, curved-collision and generated-nonfinite risks in the first pass. Regression tests reproduced a false collision on a clear curved arc, an overflow path and a long-horizon tolerance miss before the fixes. The corrected code carries the finer comparison trajectory from the initial epoch through the full coverage and rejects a 600 s, one-year request for a 100 m bound.

`cmake --build build/m2 --config Release` and `ctest --test-dir build/m2 -C Release --output-on-failure` passed with Visual Studio C++. The same source configured and passed with MSYS2 UCRT64 GCC 16.2 and Ninja. The test executable reports **51 checks**.

| Synthetic result | Measured |
|---|---:|
| Two-body circular pair, ~1 year, 600 s step: largest relative position error | 4,658.03 m |
| Same: largest relative velocity error | 0.000890201 m/s |
| Position-error ratio at 600 s versus 300 s | 3.9999 |
| Coarse versus 300 s run, sampled across whole year | 3,498.77 m and 0.000676087 m/s |
| Three-body 30 days: center-of-mass drift | 3.35844e-9 m |
| Three-body: relative momentum, angular-momentum, energy drift | 1.12984e-15, 2.54218e-15, 3.67459e-15 |
| Three-body versus independently written RK4 force loop, sampled every 5 days | 173.978 m and 2.48836e-5 m/s |

The independent reference shares the fixture values and vector operators but has a separate force summation and RK4 advance. The 100 m/0.01 m/s fitted-reference limit passes for a short synthetic interval; a one-year 600 s run needs a looser explicit position bound or a smaller step. The cache records the measured maximum against a continuously propagated sixfold finer reference and rejects a requested bound it misses.

## M2 boundary still open

- The M1 JNSQ catalog is correctly rejected: it lacks complete gravitational parameters, a common Cartesian state epoch and validated inertial frame. No JNSQ ephemeris has been produced.
- The current cache is in memory. A persisted, versioned snapshot/cache file and its reload validation are still needed for repeatable mission runs.
- The collision safety bound avoids claiming a clear chord is a collision and forces ambiguous close approaches into smaller substeps or a failure. The local boundary time uses a constant-acceleration approximation at the final resolution; an independent close-encounter trajectory comparison is still needed before mission-clearance claims.
- The current frame API trusts an upstream inertial-frame assertion. A runtime exporter and recorded transform provenance must be added before a real KSP/Principia snapshot can be integrated.
- Native Ubuntu build and execution of this C++ core are pending the new CI result. KSP and Principia comparison has not been attempted.
