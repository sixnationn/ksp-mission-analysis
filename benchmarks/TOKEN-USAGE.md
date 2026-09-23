# Token benchmark

Registered task sessions only; active response is incomplete until next refresh.

Input includes cached input. Reasoning is a subset of output. Total is input plus output; do not add subsets again. Uncached input includes any cache-write input; cache writes are also reported separately.

Recorded runtime counters, not an invoice or weekly-limit estimate. Model attribution follows turn_context. No timing or quality claim follows from token counts alone.

| Model | Effort | Sessions | Fresh input | Cached input | Output | Reasoning subset | Total |
|---|---|---:|---:|---:|---:|---:|---:|
| gpt-6-astra | high | 1 | 54319 | 326528 | 2325 | 175 | 383172 |
| gpt-6-astra | medium | 1 | 206571 | 2700032 | 15499 | 852 | 2922102 |
| gpt-6-sol | high | 3 | 944719 | 36668416 | 114405 | 56862 | 37727540 |
| gpt-6-sol | medium | 2 | 1509865 | 129252864 | 495124 | 128736 | 131257853 |
| gpt-6-sol | xhigh | 1 | 2926022 | 268213632 | 761457 | 326134 | 271901111 |

Captured UTC: 2026-09-23T08:40:14.1636591Z

See token-usage.csv for per-session roles, source paths and each counter timestamp. Refresh next turn to include the coordinator final response.
