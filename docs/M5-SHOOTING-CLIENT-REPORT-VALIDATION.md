# M5 shooting client and report bridge validation

The failure cases in `M5-SHOOTING-CLIENT-REPORT-CONTRACT.md` were specified before implementation on 23 September 2026. The focused tests were red at compile time before production code because the bounded shooting client kind and report APIs were absent. A later claim-in-target-state regression was red before its validator fix.

`WorkerClient` now accepts the bounded `shoot_route` protocol with exact source identity, runtime frame and epoch, SI vectors, bounded monotonic probe progress, one terminal and explicit cancellation. It rejects extra fields, including alternate verification claims nested in the final launch or checkpoint target states. An accepted result requires complete strict coarse/fine passes and burn/checkpoint consistency. Diagnostic completions retain their non-success label and cannot carry strict data.

Shooting reports embed and rehash the exact runtime snapshot bytes. Validation binds the request to parsed source epoch, body roles, fixed stay, launch state, target states, seed and final impulse caps, terminal status and reported burns. The report round-trip preserves accepted and diagnostic results; invalid saves leave the last valid file intact. Report loading checks provenance and structure but does not repropagate the trajectory.

The Sol High review found four P2 acceptance gaps: incomplete strict-pass fields, incomplete request validity, cancellation count regression and alternate verification claims in unknown fields. A follow-up recheck found the same claim path inside nested target states. All five were fixed, with no remaining P1/P2 finding in the final recheck.

Focused WorkerClient and report tests passed 2/2 on MSYS2 UCRT64 and 2/2 on MSVC Debug. After the final nested-state fix, each focused test passed again on both toolchains. Full local MSYS2 build, including desktop and worker targets, succeeded; CTest passed 14/14. [GitHub Actions run 35832812678](https://github.com/sixnationn/ksp-mission-analysis/actions/runs/35832812678) passed all six Ubuntu 24.04 and Windows 2022 jobs for source commit `0350935`, including both desktop builds and their full CTest suites.

This slice validates a manufactured runtime-shaped fixture and headless desktop/report behavior. It does not establish a feasible real JNSQ Reborn route, installed KSP/Principia agreement, route-seed evidence, continuous Mars-stay clearance or the rendered GTK workflow. GTK wiring, real-runtime comparison and scenario acceptance remain open.
