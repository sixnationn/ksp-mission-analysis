# M3 spacecraft propagation contract

Specified before M3 implementation. This stage consumes only an M2 ephemeris whose input snapshot passed the complete Cartesian-state gate. A raw JNSQ config catalog cannot be used as a planetary trajectory. The spacecraft is a massless test particle in the same inertial frame and SI-second UT scale as the ephemeris.

## Dynamics and events

At time `t`, acceleration is the sum of `mu_body * (r_body(t)-r_sc(t)) / |r_body(t)-r_sc(t)|^3` over the massive bodies. The force model remains continuous across sphere-of-influence boundaries. Adaptive integration has separately recorded absolute position and velocity tolerances, relative tolerance, maximum/minimum step, accepted/rejected step counts and ephemeris identity. It must stop outside ephemeris coverage or when the requested tolerance cannot be met. Its dense interpolation or a bracketed reintegration locates events, rather than accepting a grid point as the event time.

An impulse is an instantaneous velocity change at an explicit UT, applied once after the pre-burn coast has reached that exact time. The state before and after shares a position; velocity differs by exactly the requested vector. Coast intervals split at every burn. The report retains vector delta-v and its magnitude separately. A requested burn inside a body or outside coverage fails.

The first crossing into `radius + atmosphere boundary + user safety margin` is a collision/unsafe-entry event. Closest approach is a root of relative position dot relative velocity, with approach-to-recede direction checked. Event time, body ID, miss distance and reference frame are reported. SOI and display camera changes cannot alter the integrated path.

## Observable failures to test first

| Case | Required result |
|---|---|
| Provisional M1 catalog, unresolved M2 input, missing ephemeris coverage, mismatched epoch/frame/force model | Reject before propagating. |
| NaN/Inf state, negative margin, nonpositive tolerance or step bounds, zero/negative body clearance | Reject with field-specific error. |
| Initial state inside a body or atmosphere safety surface, or crossing that surface between accepted steps | Report first unsafe event and stop; do not publish a successful transfer. |
| Burn at start, at end, two burns at the same UT, burn outside coverage, or burn after collision | Apply ordered, documented semantics or reject ambiguity; never lose or duplicate a burn. |
| Integrator step straddles a burn, closest approach or coverage end | Split/locate the event at its actual UT; do not round it to the step boundary. |
| Adaptive step falls below minimum or cannot satisfy declared tolerance | Fail explicitly with last valid state and diagnostic. |
| Closest-approach tangent, multiple encounters, or an encounter at a segment boundary | Report unambiguous roots or a diagnostic; do not infer a flyby from a sampled minimum alone. |

## Acceptance evidence

1. Two-body circular and eccentric Kepler coast: compare position/velocity after one period with analytic elements and an independent propagation method. Report SI absolute maxima and requested/actual tolerances.
2. Hyperbolic two-body pass: compare asymptotic speed and turning angle with analytic scattering values; reject a periapsis below clearance.
3. Moving three-body reference: compare against an independently evaluated force law with tighter integration; disclose shared code and ephemeris fit error.
4. Exact impulse: use a burn at an off-grid time and verify position continuity, exact vector velocity jump and correct post-burn orbit. Test start/end and collision-adjacent events.
5. Event detection: bracket a deliberately between-step closest approach and unsafe-entry crossing, then verify time and distance against a tighter reference. Camera and display sampling changes must leave the numerical state unchanged.

Synthetic tests validate the propagator. The KSP/Principia accuracy budget and trajectory comparison remain a separate gate that requires a runtime snapshot and matching force settings.
