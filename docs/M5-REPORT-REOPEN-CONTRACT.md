# M5 saved-result reopen contract

Specified on 23 September 2026 before implementation. The desktop can save screened studies, fixed-impulse evaluations and bounded-shooting reports, and the report libraries can validate saved documents. The GTK workflow cannot reopen a saved result. This slice adds read-only reopening against a currently imported exact runtime source, without starting KSP, a worker or another project window.

## Source and report authority

Use a bounded report file read and parse once. Detect exactly three formats: a schema-1 historical screened study (no `report_kind`), `fixed_impulse_evaluation`, and `bounded_shooting`. Run the corresponding existing report validator on the parsed document before exposing any result. Reject unknown kinds, malformed/truncated or oversized JSON and unsupported schema. Require `runtime_observed_uncompared` and an exact snapshot SHA-256 match to the currently imported runtime scene. Preserve its frame and state epoch; a report from a different source must not silently replace the scene or result. The historical study contains source identity but not the source bytes; requiring a current import is intentional. Fixed and shooting reports contain exact bytes, but this slice still binds their display to the current import for a uniform source rule.

Add **Open saved report** using the existing report-path field. It is read-only: display a validated result and its report kind/source label, disable Save until a new worker completion, and leave search/evaluation/shooting actions available for the imported source. A failed open leaves the previous scene and displayed result intact and gives a visible reason. Starting a worker or importing another source replaces the historical display according to existing workflow rules. No report can claim installed-game/Principia validation or an optimizer result merely by being reopened.

Screened studies show their historical status and ranked route count with the patched-conic screen-only label; they do not become accepted trajectory trials. Fixed evaluations show the already validated checkpointed trial metrics and false evidence flags. Bounded shooting shows the accepted or diagnostic status through the existing presenter, with a diagnostic still labeled non-success. Do not repropagate on report load or reinterpret stored SI UT as a calendar epoch.

## Observable failures before implementation

| Case | Expected observation |
|---|---|
| Valid saved study, fixed evaluation or bounded-shooting report with the currently imported exact source | Bounded load and existing validation succeed; correct historical kind/result appears; no worker starts and Save stays disabled. |
| Different snapshot hash, confidence, frame or epoch | Load rejects visibly; previous source and result remain intact. |
| Unknown kind/schema, truncated/oversized file, tampered embedded bytes, moved target, altered shooting status/label or fabricated verification flag | Load rejects through dispatch or the owning report validator before display. |
| Diagnostic bounded-shooting completion | Reopens as non-success with coarse data; never as checkpointed acceptance. |
| No imported runtime source or active worker | Open action does not replace the current view or race worker state. |

Acceptance requires red-before-code headless tests for report dispatch and current-source binding, focused MSYS2/MSVC checks, full CTest and Ubuntu/Windows CI. GTK remains unopened on the user's main desktop. Rendered review, real-runtime workload responsiveness and installed KSP/Principia comparison remain separate gates.
