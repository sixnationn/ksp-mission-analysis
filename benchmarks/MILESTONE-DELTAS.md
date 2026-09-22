# Recorded token deltas

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

## M1 checkpoint to M2 kernel checkpoint

The M2 kernel checkpoint in `milestones/m2-kernel.json` was captured on 22 September 2026 at 20:39:55 UTC. This interval also includes the GitHub migration, M2 contract, CI setup and later conversation activity; it is not an isolated model benchmark.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 22,266,118 | 21,966,720 | 299,398 | 116,809 | 58,254 |
| gpt-6-sol | medium | 1,920,753 | 1,880,320 | 40,433 | 17,263 | 3,247 |
| gpt-6-sol | high | 580,564 | 543,488 | 37,076 | 8,119 | 4,526 |

Astra and Luna recorded no additional tokens in this interval. Input includes cached input; reasoning is a subset of output. The interval ends before this response and cannot be converted into an account-usage or billing figure.

## M2 kernel to M4 screen checkpoint

The `milestones/m4-screen.json` checkpoint was captured at 21:25:35 UTC. This interval contains M3 spacecraft propagation, the M4 Lambert/flyby/grid screen, runtime snapshot and exporter work, plus coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 28,148,807 | 27,776,384 | 372,423 | 98,172 | 37,765 |
| gpt-6-sol | medium | 9,041,449 | 8,939,392 | 102,057 | 55,783 | 13,583 |
| gpt-6-sol | high | 1,065,945 | 1,022,720 | 43,225 | 11,246 | 6,816 |

## M4 screen to M5 import checkpoint

The `milestones/m5-import.json` checkpoint was captured at 21:59:35 UTC. It includes M4 terminal-position refinement, the synthetic worker, the first rendered GTK slice and runtime JSON import, supplied game-mod diagnostics, and intervening conversation. The user-supplied EVE preview and Parallax texture downloads are not token charges; these rows are model runtime counters only.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 25,277,522 | 25,023,104 | 254,418 | 60,309 | 19,579 |
| gpt-6-sol | medium | 15,558,076 | 15,413,760 | 144,316 | 52,174 | 16,652 |
| gpt-6-sol | high | 930,123 | 910,976 | 19,147 | 4,971 | 2,271 |

Astra and Luna have no additional recorded tokens in either interval. Total input already includes cached input, and reasoning is a subset of output. These are recorded tokens from registered sessions, not billing, account usage percentage or a price estimate. The active coordinator's final response is included only after the next turn refreshes the ledger.
