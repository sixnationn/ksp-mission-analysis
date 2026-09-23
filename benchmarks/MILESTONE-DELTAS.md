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

## Reusable route-trial probe checkpoint

The `milestones/m4-route-probe.json` checkpoint was captured at 04:32:52 UTC on 23 September 2026, against `milestones/m4-route-probe-start.json` from 04:17:33. This interval includes the prewritten probe contract, implementation, numerical review, local tests, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 8,067,020 | 8,010,368 | 56,652 | 14,083 | 6,358 |
| gpt-6-sol | medium | 3,977,466 | 3,948,928 | 28,538 | 10,539 | 3,204 |
| gpt-6-sol | high | 2,080,507 | 2,033,280 | 47,227 | 6,292 | 3,550 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Bounded fixed-route shooting checkpoint

The `milestones/m4-shooting.json` checkpoint was captured at 05:33:55 UTC on 23 September 2026, against `milestones/m4-shooting-start.json` from 05:18:33. This interval includes the prewritten shooting contract, implementation, numerical review, local MSYS2 and MSVC checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 4,752,126 | 4,680,192 | 71,934 | 13,973 | 6,270 |
| gpt-6-sol | medium | 5,087,953 | 5,038,464 | 49,489 | 11,726 | 3,834 |
| gpt-6-sol | high | 457,366 | 447,360 | 10,006 | 2,002 | 1,471 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Headless bounded-shooting worker checkpoint

The `milestones/m4-worker-shooting.json` checkpoint was captured at 06:37:41 UTC on 23 September 2026, against `milestones/m4-worker-shooting-start.json` from 06:19:04. This interval includes the prewritten worker contract, callback and protocol implementation, numerical/import review and fixes, local MSYS2 and MSVC checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 8,473,006 | 8,409,088 | 63,918 | 13,307 | 5,419 |
| gpt-6-sol | medium | 4,530,433 | 4,472,704 | 57,729 | 15,523 | 3,186 |
| gpt-6-sol | high | 2,945,673 | 2,912,000 | 33,673 | 5,823 | 3,185 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Shooting client and report bridge checkpoint

The `milestones/m5-shooting-client-report.json` checkpoint was captured at 07:45:13 UTC on 23 September 2026, against `milestones/m5-shooting-client-report-start.json` from 07:19:42. This interval includes the prewritten client/report contract, implementation, import-contract review and fixes, local MSYS2 and MSVC checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 7,702,989 | 7,625,088 | 77,901 | 14,870 | 5,791 |
| gpt-6-sol | medium | 11,286,435 | 11,203,328 | 83,107 | 31,001 | 4,545 |
| gpt-6-sol | high | 5,677,889 | 5,615,616 | 62,273 | 7,631 | 3,147 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Bounded-shooting GTK workflow checkpoint

The `milestones/m5-gtk-shooting-workflow.json` checkpoint was captured at 08:40:14 UTC on 23 September 2026, against `milestones/m5-gtk-shooting-workflow-start.json` from 08:20:13. This interval includes the prewritten GTK shooting contract, implementation, headless request and presentation tests, local MSYS2 and MSVC checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 7,665,556 | 7,580,160 | 85,396 | 15,124 | 7,415 |
| gpt-6-sol | medium | 6,628,748 | 6,554,368 | 74,380 | 18,178 | 3,668 |

Sol high, Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Saved-result reopen checkpoint

The `milestones/m5-report-reopen.json` checkpoint was captured at 09:01:56 UTC on 23 September 2026, against `milestones/m5-report-reopen-start.json` from 08:42:43. This interval includes the prewritten reopen contract, implementation, import/source review, local MSYS2 and MSVC checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 9,687,471 | 9,653,376 | 34,095 | 11,077 | 4,312 |
| gpt-6-sol | medium | 5,770,218 | 5,708,160 | 62,058 | 20,384 | 6,015 |
| gpt-6-sol | high | 1,946,876 | 1,923,968 | 22,908 | 2,486 | 1,223 |

Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## M5 tester handoff checkpoint

The `milestones/m5-tester-handoff.json` checkpoint was captured at 09:43:37 UTC on 23 September 2026, against `milestones/m5-tester-handoff-start.json` from 09:24:25. This interval includes public repository visibility, tester guide and artifact staging, local failure checks, a Windows CI packaging fix, Ubuntu/Windows CI and artifact inspection, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | xhigh | 9,016,219 | 8,904,320 | 111,899 | 26,473 | 11,663 |
| gpt-6-sol | medium | 2,370,935 | 2,354,304 | 16,631 | 6,659 | 2,566 |

Sol high, Astra and Luna recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Linux Mint view and Proton capture checkpoint

The `milestones/m5-mint-view-proton.json` checkpoint was captured at 11:21:02 UTC on 23 September 2026, against `milestones/m5-mint-view-proton-start.json` from 11:06:58. This interval includes the Linux Mint screenshot review, viewport and camera fixes, inclined five-body synthetic fixture, Proton capture guide, local checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | max | 9,438,280 | 9,349,376 | 88,904 | 25,992 | 14,955 |

Other registered model and effort combinations recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Tester build update handoff checkpoint

The `milestones/m5-updates.json` checkpoint was captured at 13:50:05 UTC on 23 September 2026, against `milestones/m5-updates-start.json` from 13:41:12. This interval includes the update-control contract, GitHub API check, GTK implementation, documentation and local tests. CI after this checkpoint is outside the interval.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | max | 3,525,297 | 3,484,928 | 40,369 | 18,028 | 10,483 |

Other registered model and effort combinations recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Linux Mint zoom and Ubuntu exporter guide checkpoint

The `milestones/m5-zoom-exporter-ubuntu.json` checkpoint was captured at 19:49:30 UTC on 23 September 2026, against the prior milestone's end snapshot, copied to `milestones/m5-zoom-exporter-ubuntu-start.json`, from 13:50:05. This contiguous interval includes the preceding final response, tester screenshot review, depth-clipping fix, Ubuntu exporter instructions, local checks, Ubuntu and Windows CI, and coordination. It is a recording interval, not isolated benchmark time.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | max | 8,463,765 | 8,236,928 | 226,837 | 20,590 | 12,081 |

Other registered model and effort combinations recorded no new tokens. Total input includes cached input, and reasoning is a subset of output. These are recorded task-session counters, not billing or account-limit usage. The active coordinator response enters on the next refresh.

## Linux Mint capture shortcut checkpoint

The `milestones/m5-capture-hotkey.json` checkpoint was captured at 20:52:52 UTC on 23 September 2026, against `milestones/m5-capture-hotkey-start.json` from 20:50:27. This interval includes the preceding answer, the capture shortcut change, documentation, local exporter compilation and coordination. The live response enters at the next refresh.

| Actual model | Effort | Total input | Cached input | Fresh input | Output | Reasoning subset |
|---|---|---:|---:|---:|---:|---:|
| gpt-6-sol | max | 803,758 | 779,136 | 24,622 | 4,689 | 2,027 |

Other registered model and effort combinations recorded no new tokens. Input includes cached input, and reasoning is a subset of output. These are recorded session counters, not billing or account-limit usage.
