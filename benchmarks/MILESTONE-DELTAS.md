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

## M5 import to M6 route-screening checkpoint

The `milestones/m6-route.json` checkpoint was captured at 22:20:29 UTC. This interval includes the reviewed synthetic route assembler, no-leap display code, runtime dependency work, CI checks, desktop placement diagnosis and intervening conversation. It is not an isolated model benchmark.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 12,436,458 | 12,308,096 | 128,362 | 41,872 | 21,782 |
| gpt-6-sol | medium | 7,374,432 | 7,317,376 | 57,056 | 29,962 | 9,151 |
| gpt-6-sol | high | 964,614 | 949,760 | 14,854 | 4,099 | 1,886 |

No Astra or Luna tokens increased in this interval. Input includes cached input; reasoning is part of output. These are recorded runtime counters, not billing or account-wide usage. The active coordinator's final response remains outside this capture until the next refresh.

## M6 route screen to M4 runtime worker checkpoint

The `milestones/m4-runtime-worker.json` checkpoint was captured at 22:33:21 UTC. It includes the runtime-snapshot worker mode, import/provenance review and fixes, CI completion, desktop-status work, and intervening conversation. The milestones record completion order rather than numerical milestone numbering.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 8,019,973 | 7,974,144 | 45,829 | 17,495 | 7,760 |
| gpt-6-sol | medium | 4,083,883 | 4,049,920 | 33,963 | 17,564 | 7,087 |
| gpt-6-sol | high | 971,388 | 951,680 | 19,708 | 3,445 | 1,981 |

No Astra or Luna tokens increased. Total input includes cached input, and reasoning is a subset of output. These are registered runtime counters, not billing or the account's weekly usage percentage. The active coordinator's final response will enter the ledger at the next turn's refresh.

## M4 runtime worker to bounded search and worker-client checkpoint

The `milestones/m6-bounded-worker-client.json` checkpoint was captured at 22:57:16 UTC. This interval includes the bounded three-leg search, headless worker client, substantive import/search reviews, the previous conversation and coordination. It is a recording interval, not isolated build-task consumption. The subsequent mission-worker implementation is outside this checkpoint.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 12,646,720 | 12,498,816 | 147,904 | 44,461 | 22,266 |
| gpt-6-sol | medium | 3,914,330 | 3,834,624 | 79,706 | 28,275 | 9,762 |
| gpt-6-sol | high | 519,144 | 509,312 | 9,832 | 1,953 | 1,181 |

No Astra or Luna tokens increased. Cached input is included in total input, and reasoning is a subset of output. These are recorded tokens from registered sessions, not billing or account-limit consumption. The active coordinator's final response enters only after the next refresh.

## Bounded search and worker client to runtime mission workflow checkpoint

The `milestones/m5-runtime-workflow.json` checkpoint was captured at 23:26:03 UTC. This interval includes the mission worker, runtime mission form, report composition and headless validation. It is a recording interval, not isolated build-task consumption.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 22,748,021 | 22,547,456 | 200,565 | 62,395 | 23,502 |
| gpt-6-sol | medium | 10,205,875 | 10,120,576 | 85,299 | 41,510 | 11,644 |

Sol High, Astra and Luna recorded no new tokens in this interval. Cached input is included in total input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator's final response enters on the next refresh.

## Runtime mission workflow to fixed-impulse checkpoint

The `milestones/m4-fixed-impulse.json` checkpoint was captured at 23:59:15 UTC on 22 September 2026. This interval includes the three-leg worker route, C3 and report changes, CI fixes, fixed-impulse n-body evaluator, review and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 23,814,894 | 23,666,304 | 148,590 | 42,949 | 15,910 |
| gpt-6-sol | medium | 7,754,177 | 7,481,984 | 272,193 | 35,230 | 11,123 |
| gpt-6-sol | high | 1,834,061 | 1,598,720 | 235,341 | 8,799 | 4,687 |

Astra and Luna recorded no new tokens in this interval. Input includes cached input; reasoning is a subset of output. These are task-session runtime counters, not billing or the account's weekly usage. The active coordinator's final response enters only after the next refresh.

## Fixed-impulse checkpoint to Mars-radius diagnostic

The `milestones/m4-mars-radius.json` checkpoint was captured at 00:08:21 UTC on 23 September 2026. This interval includes the Mars-stay radius diagnostic, numerical contract/review, local validation, CI follow-up and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 3,821,972 | 3,793,792 | 28,180 | 8,294 | 3,355 |
| gpt-6-sol | medium | 3,877,587 | 3,835,136 | 42,451 | 13,263 | 4,707 |
| gpt-6-sol | high | 2,817,136 | 2,762,496 | 54,640 | 10,122 | 4,030 |

Astra and Luna recorded no new tokens. Input includes cached input; reasoning is a subset of output. These are runtime counters from registered sessions, not billing or account-limit usage. The coordinator final response will enter at the next refresh.

## Overnight worker start to fixed-impulse worker checkpoint

The `milestones/m4-evaluator-worker.json` checkpoint was captured at 00:35:16 UTC on 23 September 2026, against `milestones/overnight-worker-start.json` from 00:13. This interval includes the protocol contract, implementation, import review, local tests, CI validation and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 10,871,935 | 10,693,248 | 178,687 | 15,056 | 6,353 |
| gpt-6-sol | medium | 8,319,119 | 8,258,944 | 60,175 | 23,461 | 5,829 |
| gpt-6-sol | high | 2,048,494 | 2,003,840 | 44,654 | 6,818 | 4,037 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or the account's weekly usage. The active coordinator response enters at the next refresh.

## Fixed-impulse worker to headless client checkpoint

The `milestones/m5-evaluator-client.json` checkpoint was captured at 01:29:40 UTC on 23 September 2026, against `milestones/m5-evaluator-client-start.json` from 01:14:05. This interval includes the event contract, client implementation, fake-process regression tests, local and Ubuntu/Windows CI validation, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 5,894,706 | 5,819,264 | 75,442 | 9,480 | 3,956 |
| gpt-6-sol | medium | 9,205,439 | 9,161,600 | 43,839 | 18,364 | 4,197 |

Sol High, Astra and Luna recorded no new tokens in this interval. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or the account's weekly usage. The active coordinator response enters on the next refresh.

## Headless client to self-contained evaluation report and GTK trial

The `milestones/m5-evaluation-report.json` checkpoint was captured at 01:54:12 UTC on 23 September 2026, against `milestones/m5-evaluation-report-start.json` from 01:32:03. This interval includes the report and UI contracts, implementation, provenance review and fixes, local worker/report/GTK checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 16,302,569 | 16,210,560 | 92,009 | 29,746 | 11,820 |
| gpt-6-sol | medium | 5,079,423 | 5,009,280 | 70,143 | 25,864 | 3,194 |
| gpt-6-sol | high | 3,302,698 | 3,268,736 | 33,962 | 7,550 | 4,024 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Persisted ephemeris cache checkpoint

The `milestones/m2-persistent-cache.json` checkpoint was captured at 02:30:06 UTC on 23 September 2026, against `milestones/m2-persistent-cache-start.json` from 02:15:06. This interval includes the prewritten cache contract, implementation, focused and local full tests, numerical/import review, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 5,513,749 | 5,372,544 | 141,205 | 16,146 | 7,262 |
| gpt-6-sol | medium | 3,930,585 | 3,887,104 | 43,481 | 14,045 | 2,774 |
| gpt-6-sol | high | 3,080,350 | 3,050,112 | 30,238 | 6,435 | 4,281 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Mission-worker ephemeris cache checkpoint

The `milestones/m2-worker-cache.json` checkpoint was captured at 03:30:18 UTC on 23 September 2026, against `milestones/m2-worker-cache-start.json` from 03:16:33. This interval includes the worker cache contract, implementation, focused and local full tests, source/provenance review, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 5,718,985 | 5,670,144 | 48,841 | 9,541 | 3,037 |
| gpt-6-sol | medium | 3,719,260 | 3,692,672 | 26,588 | 8,083 | 2,239 |
| gpt-6-sol | high | 5,075,565 | 5,029,504 | 46,061 | 5,162 | 2,324 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.
