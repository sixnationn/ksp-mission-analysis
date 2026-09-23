# Token benchmark

Registered task sessions only; active response is incomplete until next refresh.

Input includes cached input. Reasoning is a subset of output. Total is input plus output; do not add subsets again. Uncached input includes any cache-write input; cache writes are also reported separately.

Recorded runtime counters, not an invoice or weekly-limit estimate. Model attribution follows turn_context. No timing or quality claim follows from token counts alone.

| Model | Effort | Sessions | Fresh input | Cached input | Output | Reasoning subset | Total |
|---|---|---:|---:|---:|---:|---:|---:|
| gpt-6-astra | high | 1 | 54319 | 326528 | 2325 | 175 | 383172 |
| gpt-6-astra | medium | 1 | 206571 | 2700032 | 15499 | 852 | 2922102 |
| gpt-6-sol | high | 3 | 636625 | 12307968 | 66692 | 30843 | 13011285 |
| gpt-6-sol | medium | 2 | 972396 | 68025472 | 318340 | 92066 | 69316208 |
| gpt-6-sol | xhigh | 1 | 1904681 | 176478976 | 578578 | 252321 | 178962235 |

Captured UTC: 2026-09-23T00:08:21.3712202Z

See token-usage.csv for per-session roles, source paths and each counter timestamp. Refresh next turn to include the coordinator final response.
