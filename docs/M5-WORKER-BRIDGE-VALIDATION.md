# M5 worker bridge checkpoint

The headless `WorkerClient` launches the existing JSON-lines worker as a separate process, validates event version/request/source identity and bounded progress, retains a bounded event queue, and reaps the child on completion or cancellation. A watchdog terminates a child that stops reading either the initial request or cancellation control. The UI has not yet connected to this bridge.

Failure cases were written before implementation. On Windows, MSYS2 UCRT64 and Visual Studio Debug focused tests each passed 49 checks, including malformed/oversized/wrong-source events, premature EOF, nonzero exit, an unresponsive child, cancellation acknowledgment, restart, and a real synthetic worker request. The initial stalled-pipe test exceeded a controlled 12-second deadline before the watchdog was added. After hardening, the complete UCRT64 C++ suite passed 10/10 CTest targets on 22 September 2026. No GTK or KSP window was opened for this checkpoint.

This establishes process/protocol behavior on the Windows host. The six native Ubuntu/Windows CI jobs passed in run 35795955096 after the one-shot worker was changed to exit without waiting for its detached stdin reader during C++ stream teardown. GTK main-context dispatch, close-window responsiveness, rendered view, runtime route search, save/reload and reports remain open for later checkpoints.
