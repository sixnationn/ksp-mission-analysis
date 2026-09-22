# M0 and M1 recorded token deltas

Baseline: `milestones/start.json`, 2026-09-22 19:06:44 UTC. M0 checkpoint: `milestones/m0.json`, 19:26:08 UTC. M1 checkpoint: `milestones/m1.json`, 19:46:01 UTC. Actual model and effort come from session `turn_context`; all three M0/M1 sessions are registered in `sessions.json`. The reviewer is Sol high. No Astra or Luna token increase occurred after the baseline.

| Capture interval | Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---|---:|---:|---:|---:|---:|
| Start to M0 checkpoint | gpt-6-sol | xhigh | 11,336,993 | 11,222,016 | 114,977 | 39,008 | 19,447 |
| Start to M0 checkpoint | gpt-6-sol | medium | 3,660,861 | 3,560,448 | 100,413 | 22,114 | 3,445 |
| M0 to M1 checkpoint | gpt-6-sol | xhigh | 7,684,670 | 7,572,224 | 112,446 | 45,083 | 22,027 |
| M0 to M1 checkpoint | gpt-6-sol | medium | 1,606,445 | 1,591,936 | 14,509 | 5,202 | 1,665 |
| M0 to M1 checkpoint | gpt-6-sol | high | 1,591,972 | 1,476,480 | 115,492 | 9,279 | 2,723 |
| **Start to M1 checkpoint** | **gpt-6-sol** | **xhigh** | **19,021,663** | **18,794,240** | **227,423** | **84,091** | **41,474** |
| **Start to M1 checkpoint** | **gpt-6-sol** | **medium** | **5,267,306** | **5,152,384** | **114,922** | **27,316** | **5,110** |
| **Start to M1 checkpoint** | **gpt-6-sol** | **high** | **1,591,972** | **1,476,480** | **115,492** | **9,279** | **2,723** |

The M0 checkpoint was taken while the builder was already active, so interval rows reflect recording time, not exclusive work attribution. The collector differences cumulative counters per session; it does not sum repeated snapshots. Total input already includes cached input; reasoning is part of output. These are recorded runtime tokens, not billing tokens, cost, or subscription usage. The active coordinator's final response is absent until the next turn refreshes the ledger.
