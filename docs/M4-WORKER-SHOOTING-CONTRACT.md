# M4 headless bounded-shooting worker contract

Specified on 23 September 2026 before implementation. The core can repair one nearby fixed-date four-impulse seed, but the existing worker exposes only strict evaluation of a caller-supplied trial. This slice makes that bounded search callable in the one-request headless worker. It does not add date search, a UI flow, a route optimizer, or KSP/Principia validation.

## Request and source authority

Add protocol-version-1 command `shoot_route` using the existing `evaluate_route` runtime-source and `trial` schema, plus a required `shooting` object containing finite-difference impulse step in m/s, per-impulse magnitude cap in m/s, iteration cap and probe-evaluation cap. Reject unknown fields and out-of-range or nonfinite limits before emitting `started`. Read the runtime snapshot from one bounded binary file read; compare repeated bytes and exact lowercase SHA-256 before numerical work. Parse its frame, epoch, source confidence and atmosphere boundaries from those bytes. Reuse `evaluation_request` validation and `prepare_route_probe_runtime`; do not allow synthetic or provisional config through this worker command. The core's fixed launch state, target, date and source guards remain in force.

## Progress, cancellation and terminal result

Make the core shooting loop accept an optional cancellation predicate and progress callback without changing existing default-call behavior. Poll cancellation between completed coarse probe propagations and before/after strict repropagation. An individual spacecraft propagation and the one-time planetary integration may remain noninterruptible; disclose that in `started`. A cancelled run emits one `cancelled` terminal event, never `complete` or an accepted result. Emit monotonic bounded progress counts in completed probes out of the request cap, with the source hash, confidence, frame/epoch and SI-unit label. Do not emit an unbounded stream or partial success flag.

Emit exactly one bounded terminal event for a completed search. For `checkpointed_accepted`, include the final four inertial SI impulses, unchanged initial state and body-relative targets or an explicit exact trial reference, four signed coarse checkpoint residuals, observed Venus event/margin, Mars radius bounds, charged burn magnitudes, strict coarse/fine result and disagreement metrics, source/frame/epoch and the unchanged `independent_nbody_fixed_impulse_checkpointed_only` label. For non-success, include the core status, completed probes, latest safe diagnostics and `independent_nbody_coarse_trial_diagnostic_only`; do not emit a strict result or acceptance label. Preserve the strict evaluator's `route_seed_evidence_revalidated=false` and `mars_stay_continuously_verified=false` limits. No `optimized`, `global_optimum`, `principia_matched` or `runtime_verified` field.

## Observable failure cases before implementation

| Case | Expected worker observation |
|---|---|
| Runtime-shaped manufactured route with a nearby four-burn perturbation | `started`, monotonic bounded progress, one `complete` with `checkpointed_accepted`, final physical impulses and strict result tied to exact hash, frame and epoch. |
| Exact seed with shifted Venus window or impossible encounter radius | `complete` diagnostic non-success with no strict result or accepted label. |
| Tight strict disagreement budget | Coarse pass followed by `strict_rejected` diagnostic, never accepted. |
| Cancellation before numerical work, between probes, and after strict work | Exactly one `cancelled` terminal; no later `complete`. Completed-probe count does not exceed the cap. |
| Stale hash, altered same-length bytes, wrong frame/epoch, malformed vector, over-limit seed, hidden atmosphere/source-confidence claim, invalid cap or extra field | Bounded pre-start error; no numerical success event. |
| Runtime atmosphere boundary larger than the actual Venus clearance despite caller claims | Unsafe/error terminal, no accepted or diagnostic success claim. |

Acceptance is a red-before-code worker/core test, focused local MSYS2 and MSVC tests, full local CTest, Ubuntu/Windows CI and a read-only numerical/import contract review if the core callback changes. The fixture is manufactured. It does not establish a feasible JNSQ Reborn route or installed-game agreement.
