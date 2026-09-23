# M5 shooting client and report bridge contract

Specified on 23 September 2026 before implementation. The headless worker now emits `shoot_route` progress and a completed accepted or diagnostic result. The desktop worker client currently understands screened routes and fixed evaluation only, and the report writer cannot save shooting results. This slice validates that event stream and saves a self-contained completed shooting report. It does not add GTK controls or claim a real JNSQ/Principia comparison.

## Client event validation

Add an explicit shooting result kind to `WorkerEventValidator` and `ClientOptions`; existing result kinds must keep their behavior. Require matching protocol version, request ID, exact snapshot hash, `runtime_observed_uncompared` source confidence, SI units, right-handed inertial runtime frame and finite epoch. `started` has the coarse diagnostic label. Allow only `coarse_probes` progress with a stable positive cap at most 10,000, nondecreasing completed attempt counts no greater than the cap, and a bounded number of progress events. Support cancellation before `started`, between probes and after strict work, with one terminal event. Reject any event after a terminal.

A `complete` event must match the started frame/epoch and preserve false route-seed and continuous-Mars-verification flags. Check four ordered SI impulses, fixed 5,184,000-second Mars stay, unchanged target-state shape, four named signed coarse residuals, burn vector/magnitude consistency, finite Mars radius interval and optional actual Venus event. For `checkpointed_accepted`, require the fixed-impulse checkpointed label, a present strict coarse/fine result, four matching impulses and epochs, positive Venus clearance and finite disagreement/charged delta-v. For every other allowed completion status, require the coarse diagnostic label and no strict result. Reject invented success/verification flags and malformed or oversized payloads. The client validates reported structure and arithmetic; it does not independently integrate the trajectory.

## Self-contained report

Provide separate compose/validate/atomic save/load functions for completed `shoot_route` results. Store the exact runtime snapshot JSON bytes and SHA-256, full request including source frame/epoch and bounded shooting limits, all validated events, final trial, outcome label and explicit false evidence flags. Validation must rehash and parse the embedded runtime source; tie request hash, frame, epoch, role identities, fixed stay, launch state and checkpoint targets to the terminal final trial. Tie reported burn vectors and totals to final impulses. A diagnostic completion is saveable with its non-success status; cancellation, error, a missing terminal or a claim of feasibility, optimization, installed-game verification or Principia equivalence is not. Reject changed source bytes, moved targets, altered labels/status, missing strict data on accepted outcomes, fabricated strict data on diagnostics, progress regression and duplicate terminal events. Save must replace atomically and preserve the last good report if validation fails.

## Observable failure cases before implementation

| Case | Expected observation |
|---|---|
| Valid runtime-shaped worker accepted stream | Client accepts bounded progress and one terminal; report round-trips and retains exact bytes/hash, final impulses, strict result and source frame/epoch. |
| Valid missing-Venus or strict-rejected diagnostic completion | Client and report retain non-success status with no strict result or accepted label. |
| Cancel before start, between probes or after strict | Client accepts one cancellation terminal; report composition rejects it as incomplete. |
| Changed hash/confidence/frame/epoch, progress count/cap regression, duplicate terminal, wrong status/label, false evidence flag, invalid SI vector, burn mismatch or added strict result to a diagnostic | Client rejects the stream before it can become a saved result. |
| Mutated embedded runtime bytes, moved fixed target or launch state, altered final impulse, invalid shooting cap, forged verification claim or truncated saved file | Report validation/load rejects; failed save leaves the previous valid file intact. |

Acceptance uses tests added before code, focused MSYS2 and MSVC checks, full local CTest and Ubuntu/Windows CI. The existing worker numerical test already replays a final trial through strict evaluation; the client/report tests need not repropagate on every load. Only the manufactured runtime-shaped fixture is validated here.
