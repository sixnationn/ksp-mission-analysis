# Benchmark protocol

This is an observational record of the KSP mission-tool task, starting with planning. It is not a controlled model comparison: the coordinator, numerical adviser and compatibility researcher do different work.

Use sessions.json to register every task/child log and role. Update status when work finishes. Run `./scripts/Update-TokenBenchmark.ps1` from the project root after meaningful milestones and at the next turn's start. The last call cannot include the response that follows it, so report a capture timestamp rather than a final total for an active session.

The script differences cumulative counters, preventing repeated token_count records from being added repeatedly. Model attribution comes from turn_context, not an agent nickname or configured preference. Cached input and reasoning output are subsets; do not add them again. Cache-write input is retained as a separate reported field. Raw token totals do not establish subscription consumption or money spent.

For future milestones record: outcome, assigned model/effort, source sessions, wall time, accepted/rejected deliverable, rework and validation evidence. Compare useful accepted work per fresh/cached/output token, not just the lowest token count. A real A/B study would require identical isolated work and would spend extra usage; it has not been started.

Verification performed: the generated per-session totals were compared with their last source counters at the captured timestamps; input/cached/fresh/output reconciliation passed for all three registered sessions. The initial collector encountered a Windows sharing lock; shared log reading fixed it. Missing logs or reset counters produce explicit warnings. Local raw conversations are not copied into this repository.
