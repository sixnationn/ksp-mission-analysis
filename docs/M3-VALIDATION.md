# M3 spacecraft validation

Windows Visual Studio C++ Release build and the focused M3 executable passed **57 checks** on 22 September 2026. The input is synthetic. No JNSQ runtime Cartesian snapshot, KSP flight or Principia trajectory has been compared.

| Check | Measured result |
|---|---:|
| Circular two-body period position and velocity error | 0.043419 m; 0.0000243935 m/s |
| Eccentric two-body period error | 0.360007 m; 0.000183804 m/s |
| Off-grid impulse versus separate RK4 reference | 0.0429419 m; 0.0000241472 m/s |
| Moving three-body versus separate-force RK4 | 0.0000569851 m; 0.00000000296974 m/s |
| Circular case local tolerance request | 0.01 m; 0.00001 m/s; relative 1e-11 |
| Circular case largest accepted local errors | 0.00596395 m; 0.00000159698 m/s |

The builder's tests covered exact ordered start, off-grid, same-UT and end impulses; Kepler and hyperbolic trajectories; unsafe entry; closest approach; and a moving three-body reference. A Sol High review then found that missing atmosphere values became zero, a curved unsafe dip could pass sparse probes, and absolute barycentric position loosened relative error tolerance under frame translation. Regressions were added and reproduced the missing-atmosphere and frame-translation failures, plus a curved Hermite path that previously returned success despite entering a body. The fixes require explicit atmosphere data for every body (zero means explicitly none), scale adaptive error by relative body distances and velocities, and use a conservative displacement bound for the interpolated ephemeris before accepting a step. The curved case now reports an unsafe entry within 0.1 s of an independently solved Hermite crossing. If an interval cannot be certified or bracketed at the event resolution, propagation fails with `unsafe interval unresolved` and a last valid state.

## Open acceptance risks

- The conservative bound is for the interpolated planetary path and model ODE from the current numerical state. M2 fit error and accumulated spacecraft error still need a mission-specific clearance budget, independent close-encounter comparison and loaded-game evidence.
- Closest approach roots still use sampled sign changes. A tangent or two roots within a sample interval can be omitted. Such event lists are provisional and must not be treated as exhaustive during assist optimization.
- The hyperbolic test checks conserved excess speed and osculating orbit parameters; it does not independently compare incoming and outgoing asymptotic velocity directions. That comparison remains open.
- Atmosphere heights are supplied to M3 settings because the M2 `Body` has no atmosphere field. The runtime snapshot bridge must provide them with provenance.
- Ubuntu native M3 build/test is pending the next CI run. No actual KSP or Principia comparison exists.

M3 is a usable synthetic propagator, but its full event and runtime acceptance gate is open. M4 may use it for provisional screening and must reject any unresolved safety event.
