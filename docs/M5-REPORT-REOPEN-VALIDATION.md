# M5 saved-result reopen validation

The failure cases in `M5-REPORT-REOPEN-CONTRACT.md` were specified before implementation on 23 September 2026. Headless three-kind and source-binding tests were red before code because `reopen_saved_report` was undefined. During implementation, the different-hash case exposed an overload collision with `std::bind`; the source-binding function was renamed and the mismatch tests passed.

**Open saved report** now uses the existing report path. A bounded single read dispatches schema-1 historical screened studies, fixed-impulse evaluations and bounded-shooting reports to their existing validators. The report's SHA-256, confidence, frame and state epoch must match the currently imported runtime scene. Historical studies have the original 16 MiB file cap; fixed and shooting reports use 32 MiB. The interface prepares a result summary before changing route rows, then displays the validated historical result read-only with Save disabled. Failed loads leave the previous source and displayed result intact; opening a report starts no worker.

The headless tests construct valid reports of all three kinds and reject a different hash, confidence, frame or epoch; unknown kind or schema; changed embedded bytes; a false verification claim; truncated input; and oversized files. A Sol High read-only source-contract review found no remaining P1/P2 issue. The reviewer did not execute tests or launch GTK.

Focused reopen CTest passed 1/1 on MSYS2 UCRT64 and 1/1 on MSVC Debug. Full local MSYS2 desktop build succeeded and CTest passed 16/16. [GitHub Actions run 35839937163](https://github.com/sixnationn/ksp-mission-analysis/actions/runs/35839937163) passed all six Ubuntu 24.04 and Windows 2022 jobs for source commit `e27ce51`, including both desktop builds and full CTest suites. No GTK, KSP or other project window was opened on the user's main desktop.

This is a read-only historical display against an already imported runtime scene. It does not repropagate trajectories on load, reproduce an installed KSP/Principia state, establish a feasible JNSQ Reborn route, or prove rendered interaction and responsiveness. Those remain first-release gates.
