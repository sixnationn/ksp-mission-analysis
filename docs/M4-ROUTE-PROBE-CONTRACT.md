# M4 reusable route-trial probe contract

Specified on 23 September 2026 before implementation. The full-route evaluator throws on the first failed acceptance condition and reintegrates the planets on every call. A bounded shooting search first needs repeatable signed residuals from safe, imperfect trials using one validated planetary ephemeris. This probe supplies diagnostics only. It does not optimize, certify feasibility, or replace strict repropagation.

## Inputs and outputs

Prepare an immutable probe context once from either a `synthetic_fixture` Snapshot or exact runtime snapshot bytes plus expected SHA-256. Validate source hash/confidence, right-handed inertial frame, epoch, body roles, atmosphere boundaries, route dates, settings and planetary fit budget using the full-route request contract. Integrate the coarse independent Newtonian ephemeris once. A runtime context always takes atmosphere boundaries from the parsed runtime bytes, never from a trial override. The trial type contains only a physical launch parking state, four inertial SI impulse vectors and four explicit body-relative checkpoint targets. Route identity, dates, roles, frame, epoch, settings and source provenance are fixed by the context, so a trial cannot silently alter them. The context's ephemeris/body order and metadata must remain consistent with the source.

For each safe trial, propagate the massless spacecraft over the complete route with exactly four impulses at launch, Mars arrival, Mars departure 5,184,000 SI seconds later and home return. Record the four named checkpoint states, body-relative states and **signed** position and velocity residual vectors against the supplied targets. Record signed parking radius, radial-velocity and tangential-speed residuals in SI units; four burn records and charged magnitudes; Mars-stay radius extrema; actual Venus closest-approach roots, a selected event inside the screened window when one exists, and the minimum observed Venus boundary margin. Keep missing in-window Venus as an explicit missing diagnostic. A trial that is safely propagated but misses a parking, target, Venus-window or positive safety-margin requirement returns residuals; it is not labeled accepted. Collision, unresolved propagation or an unbounded step count fails explicitly with no partial success claim.

The result label is `independent_nbody_coarse_trial_diagnostic_only` and carries snapshot hash, source confidence, frame/epoch, coarse ephemeris metadata, accepted/rejected step counts and exact trial inputs. No `feasible`, `optimized`, `principia_matched` or `runtime_verified` flag is emitted. Only `evaluate_fixed_route_runtime` or `evaluate_fixed_route_synthetic_fixture` can decide checkpointed acceptance after a separate tighter ephemeris and spacecraft repropagation; their existing behavior must remain unchanged.

## Observable failure cases before implementation

| Case | Required observation |
|---|---|
| Manufactured safe synthetic trial from the evaluator fixture | Repeated probes from one context return identical four checkpoint residuals, burn records, Mars extrema and Venus event; baseline residuals are near zero and existing strict evaluator still passes. |
| Change a finite impulse so Mars or home misses its target without collision | Probe returns a signed miss and diagnostic label; strict evaluator rejects the same fixed trial. No false acceptance flag. |
| Shift the expected Venus window so a safely propagated trial has no in-window root | Probe records missing encounter rather than inventing one; strict evaluator rejects. |
| Unsafe body/atmosphere crossing or unresolved propagation | Explicit failure with no queryable successful result. |
| Nonfinite state/impulse/target, body-centre launch, invalid baseline settings, stale hash/frame/epoch or altered baseline route/role/date/atmosphere | Reject before trial propagation. Runtime atmosphere overrides cannot replace parsed source boundaries. |
| Context ephemeris/body identity mismatch or insufficient coverage | Reject rather than using stale planetary samples. |
| More than four burns, hidden powered Venus impulse, or altered Mars stay duration | Unrepresentable in the fixed trial type; invalid baseline requests reject before preparation. |

Acceptance uses the existing manufactured full-route fixture, a focused regression test that proves a previously thrown safe miss now yields diagnostics, local MSYS2 and MSVC tests, and Ubuntu/Windows CI. No real JNSQ Reborn trajectory or installed KSP/Principia comparison is claimed.
