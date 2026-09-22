# M2 planetary ephemeris contract

Drafted before M2 implementation. M2 consumes a **complete Cartesian snapshot** in SI units. The M1 JNSQ Reborn catalog has no common Cartesian states, gravitational parameters for every body, or resolved frame, so it must fail the M2 entry gate. Synthetic fixtures are valid for numerical tests only; they do not promote an import to runtime-verified status.

## Input and output

The numerical core accepts a finite set of massive point bodies. Each has a stable ID, positive finite gravitational parameter in m^3/s^2, positive finite radius in m, and finite position/velocity at one explicit epoch in an inertial, right-handed frame with named axes and origin. All bodies use the same frame and epoch. Coverage and query times are SI seconds on the same UT scale as `state_epoch_ut_s`. An accelerated or rotating origin is rejected unless a recorded time-dependent transform converts every state to the declared inertial frame. The source confidence, snapshot hash, frame definition, force-model identity, integrator identity/version, step, coverage interval, fit settings and measured fit errors travel with the ephemeris. An independent Newtonian point-mass result is labeled `independent_newtonian_nbody`; it is never Principia-matched by name alone.

The center of mass is interpreted using masses proportional to gravitational parameters. Frame translation is allowed only when recorded; rotation and handedness conversion need explicit transforms. The integrator uses pairwise Newtonian acceleration `a_i = sum(j != i) mu_j (r_j-r_i)/|r_j-r_i|^3`. It must not add an unrecorded softening length or silently switch to patched conics. Fixed-step velocity Verlet is the first conservative method candidate. Positions and velocities are double precision. The requested interval includes both endpoints, with no silent extrapolation.

The reusable ephemeris is piecewise and stores position and velocity at segment boundaries plus enough coefficients or samples to evaluate both off-grid. Its interpolation method and segment spacing are explicit metadata. A fit must be checked at times **not used to construct it**, against a tighter independently propagated trajectory. Adjacent segments agree at their shared endpoint in both position and velocity. A query outside coverage fails.

## Failure cases specified before code

| Condition | Required observable result |
|---|---|
| M1 raw catalog, missing state/epoch/frame/axis/handedness/positive mu, mixed epochs, non-finite value, duplicate ID or fewer than two bodies | Reject before integration with a field-specific error; never set `analysis_ready=true` for the catalog. |
| Same-position bodies, surface overlap, or a later center separation crossing summed reference radii, including between fixed steps | Reject/stop at the first boundary crossing using a bracketed event or conservative swept check; report body pair and event time, never divide by zero or write a successful cache. |
| Nonpositive step, reversed coverage, interval not an integer number of steps without an explicit final-step policy, or excessive requested step count | Reject rather than silently changing the resolution or exhausting memory. |
| Query before or after coverage, NaN time, or missing body ID | Fail explicitly; no extrapolation or fallback to a different body. |
| Snapshot hash, frame, force model, integrator identity/version, step, coverage, interpolation method/settings or fit tolerance differs from cached metadata, or stored error bounds are invalid | Treat cache as a miss; never reuse trajectories under changed physics or requirements. |
| Fit misses declared position/velocity error bound at independent off-grid samples | Refine segment spacing or fail; never publish an unvalidated fitted cache. |
| Request to call the independent result `principia_matched` without installed-version comparison evidence | Reject the mode label. |

## Numerical acceptance evidence

1. **Two-body analytic orbit.** Use a barycentric circular pair with known relative angular frequency `sqrt((mu_1+mu_2)/r^3)`. With a 600 s step over one orbit at about 1 AU, require relative position error below 10 km and relative velocity error below 0.01 m/s; record actual maxima at several phases and the final phase. Run at `h` and `h/2`; for errors comfortably above roundoff, require the position error ratio to fall between 3 and 5 before choosing a production step.
2. **Closed three-body fixture.** Over a stated 30-day stable synthetic interval with 600 s step, report center-of-mass position/velocity, linear and angular momentum and energy against the initial state. First acceptance limits: center-of-mass position drift below 1 m after removing its initial uniform velocity; momentum drift below 1e-10 of the sum of initial characteristic momentum magnitudes; angular momentum relative drift below 1e-9; energy relative drift below 1e-6. These are synthetic-kernel gates, not real-mission accuracy claims. Reject collision cases.
3. **Off-grid fit.** Check segment endpoints, interior fractions 1/3 and 1/2, and continuity in both position and velocity. Compare with a reference integration using a smaller step that does not share the fit samples. Report maximum absolute errors in metres and m/s. The first synthetic acceptance target is 100 m and 0.01 m/s; real mission error budgets must be set from clearance and targeting constraints after runtime comparison.
4. **Independent comparison.** Compare one nontrivial synthetic trajectory with a reference that has an independently written force evaluation or an independently generated trusted trajectory. Document its initial state, epoch, frame, tolerances, maximum disagreement and any shared code. This establishes numerical behavior only, not KSP or Principia equivalence.

M2 cannot pass for the JNSQ system until a resolved or runtime-exported Cartesian snapshot supplies the missing state, frame and gravitational parameters. The synthetic tests prove the numerical kernel, and the JNSQ import rejection proves the gate.
