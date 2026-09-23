# M4 bounded fixed-route shooting contract

Specified on 23 September 2026 before implementation. This slice repairs a nearby four-impulse seed for one already screened route. Dates, body roles, fixed 5,184,000-second Mars stay, initial parking state, four body-relative targets, source bytes/hash, frame, epoch, atmosphere, and integration settings remain fixed. It is a local fixed-date search, not a global optimizer, a date search, or evidence of agreement with installed KSP/Principia.

## Inputs and method

Use an immutable `RouteProbeContext` prepared from a manufactured synthetic snapshot or exact runtime JSON bytes and expected SHA-256. Accept a `RouteProbeTrial` seed and finite positive bounds for finite-difference impulse step, maximum impulse magnitude, maximum iterations and maximum probe evaluations. Reject nonfinite limits, zero budgets, and a seed outside the impulse bound. Do not vary checkpoint targets or the launch parking state to reduce an error.

Use deterministic block shooting in the same SI inertial frame: solve a 3-by-3 finite-difference position Jacobian for the launch impulse against the Mars-arrival signed position residual; correct the Mars-capture impulse against the post-capture signed velocity residual; solve the Mars-departure impulse against the home-return signed position residual; correct the home-capture impulse against the post-capture signed velocity residual. Reprobe after each accepted change. Bound each impulse and every evaluation. Use a damped line search that accepts only a smaller relevant residual. Singular Jacobians, unsafe finite-difference trials, failed line searches and budget exhaustion must yield an explicit non-success result or error; no partial result may be labeled accepted.

Before strict evaluation, check all four coarse checkpoint position/velocity and parking residuals, the Mars-stay radius bounds, an actual Venus closest-approach root inside the fixed window, the selected Venus radius cap and the observed safety boundary margin. The search may return a diagnostic non-success when fixed geometry prevents these conditions. A candidate that passes the coarse gate must be repropagated by the existing strict evaluator against the same trusted snapshot and all original acceptance budgets. Only a successful strict result may be returned as a checkpointed acceptance. Preserve the existing strict evaluator behavior and its `independent_nbody_fixed_impulse_checkpointed_only` label. No `optimized`, `globally_optimal`, `principia_matched`, `runtime_verified` or continuously certified Mars-stay claim is allowed.

## Observable failure cases before implementation

| Case | Expected observation |
|---|---|
| Manufactured four-burn fixture, exact seed | Zero or bounded iterations, strict checkpointed acceptance, source/frame/epoch provenance retained. |
| Small finite launch and departure impulse offsets, with compensable capture and return offsets | Deterministic signed residual reduction and a strictly repropagated accepted trial within evaluation and impulse caps. Targets, initial state, dates and roles remain unchanged. |
| Venus window shifted away from every actual root, or an impossible radius cap | Finite non-success with an explicit missing/failed Venus diagnostic; no fabricated encounter or acceptance. |
| Coarse target appears within tolerance but strict disagreement/acceptance budget is deliberately tightened | Strict rejection remains non-success; no coarse-only success label. |
| Nonfinite setting, zero cap, over-limit seed, singular/unsafe finite-difference trial, or exhausted evaluation budget | Explicit rejection/non-success with bounded work; never an accepted partial trajectory. |
| Runtime-shaped exact bytes with valid hash and altered caller atmosphere claim | Search uses parsed source atmosphere authority; stale hash and unsafe source boundaries are rejected. |

Acceptance evidence is focused tests red on missing shooting behavior before implementation, local MSYS2 and MSVC checks, full local CTest, Ubuntu/Windows CI and a read-only Sol High numerical review. The manufactured fixture demonstrates local recovery only. A real JNSQ Reborn solution and installed KSP/Principia comparison remain separate gates.
