# KSP mission analysis

Read C:/Users/HeyTh/AGENTS.md once for the shared policy. Read docs/PLAN.md for the product and acceptance gates, and docs/ORCHESTRATION.md for model routing.

The current request authorizes planning and project agent setup. Do not treat the plan as authorization to start implementation, install dependencies, download the game, or launch unattended jobs.

Sol xhigh is the default coordinator; Sol medium builds, Sol high reviews, and Luna low runs prescribed mechanical checks. Reserve Astra high for one bounded unresolved numerical decision or failed high-effort Sol attempt, with at most one Astra active. Do not fan out Astra agents or automatically run Astra at each milestone. Use at most two children and prefer one builder, adding a reviewer only for substantive numerical or shared-contract changes. Brief with fork_turns="none" and named file ownership. Honor any later explicit user change of scope.

Keep SI units and explicit epochs/reference frames in the numerical core. Preserve imported configuration provenance. Never label an independent n-body approximation as Principia-equivalent without comparison evidence. Test failure cases before implementation. Distinguish numerical validation, rendered inspection, and actual KSP comparison.

This task is a token benchmark. Record each child in benchmarks/sessions.json with its log path and role. Run scripts/Update-TokenBenchmark.ps1 at milestone boundaries and at the next turn's start to capture the preceding final response. Report recorded per-model input, cached input, fresh input, output and reasoning subsets. Never double-count cumulative usage snapshots or invent unavailable billing/usage conversions.
