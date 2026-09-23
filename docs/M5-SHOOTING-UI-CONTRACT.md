# M5 bounded-shooting GTK workflow contract

Specified on 23 September 2026 before implementation. The worker, client validator and report writer already support `shoot_route`; the GTK window offers only screened missions and fixed-impulse evaluation. This slice exposes bounded shooting of a caller-supplied, fixed-date four-impulse trial. It does not turn screened route rows into trials, search dates, claim a global optimum or compare with installed KSP/Principia.

## User path and source authority

Add a clearly named **Shoot nearby trial** action next to the existing fixed-trial action. Reuse the advanced trial JSON path, with four visible shooting limits: finite-difference impulse in m/s, per-impulse magnitude cap in m/s, positive iteration cap at most 1,000, and positive probe cap at most 10,000. Keep the fixed 5,184,000 SI-second Mars stay and trial dates/targets from the supplied trial. The desktop must require a currently imported exact-byte runtime source, unchanged path/hash fields, a bounded and parseable trial file, and matching route-seed hash/confidence. It must reject nonfinite/nonpositive limits, over-cap integers, hidden source/atmosphere overrides and oversized request data before launching the worker. The worker retains the final authority for trial physics, frame, epoch and source re-read.

Build protocol-v1 `shoot_route` from the imported path, SHA-256, inertial frame and state epoch, the unmodified trial object and those four limits. Use `WorkerResultKind::bounded_shooting`, bounded event retention, the existing deadline and cancellation channel. Starting any worker disables competing actions and report save; importing another source clears the prior shooting result. Do not generate a route seed from a screen row or label a screen as physically accepted.

## Progress, result and report

Show completed probes over the request cap, not Lambert cells or fixed-evaluation phases. Show the worker's disclosed noninterruptible stages and preserve a responsive GTK main loop while the child runs. On `checkpointed_accepted`, show the final four burn magnitudes, signed coarse residuals, strict checkpoint/encounter metrics, exact partial-result label and both false evidence flags. For a diagnostic completion, show its status and coarse residuals with an explicit non-success label; do not display strict acceptance. Both completed statuses may be saved using `compose_shooting_report` and `save_shooting_report`, which re-open and rehash exact source bytes. Cancel, error, source replacement or invalid report must not enable a new shooting report, and a failed save must leave the prior destination intact.

## Observable failures before implementation

| Case | Expected observation |
|---|---|
| No import, stale path/hash, malformed or over-1-MiB trial, mismatched route seed, hidden source/atmosphere claim, invalid limit or over-1-MiB request | No child starts; visible rejection; previous completed result remains identifiable. |
| Valid runtime-shaped trial and four limits | One `shoot_route` request carries exact imported source, frame/epoch, unchanged trial, capped limits and a unique request ID; progress is probes out of cap. |
| Accepted completion | Four final physical burns, coarse and strict metrics, partial-result label and false evidence flags are shown; self-contained exact-byte report round-trips. |
| Diagnostic completion | Non-success status/coarse data shown and saveable; no strict acceptance or feasibility language. |
| Cancellation or worker/client error | No completed shooting report is offered; the main loop remains usable after worker reap. |
| Source file changed between import and save | Report composition rejects the changed bytes; existing output file remains intact. |

Acceptance requires red-before-code headless tests of request construction and result presentation, focused local MSYS2 and MSVC checks, full CTest and Ubuntu/Windows desktop builds. GTK must not be launched on the user's main desktop. Rendered review and responsiveness under a real numerical workload remain open M5 gates until a safe display is available. The manufactured fixture does not prove a JNSQ Reborn route or Principia equivalence.
