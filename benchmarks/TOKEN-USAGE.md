# Token benchmark

Registered task sessions only; active response is incomplete until next refresh.

Input includes cached input. Reasoning is a subset of output. Total is input plus output; do not add subsets again. Uncached input includes any cache-write input; cache writes are also reported separately.

Recorded runtime counters, not an invoice or weekly-limit estimate. Model attribution follows turn_context. No timing or quality claim follows from token counts alone.

| Model | Effort | Sessions | Fresh input | Cached input | Output | Reasoning subset | Total |
|---|---|---:|---:|---:|---:|---:|---:|
| gpt-6-astra | medium | 1 | 206571 | 2700032 | 15499 | 852 | 2922102 |
| gpt-6-astra | high | 1 | 54319 | 326528 | 2325 | 175 | 383172 |
| gpt-6-sol | high | 2 | 202802 | 3058816 | 13938 | 3465 | 3275556 |
| gpt-6-sol | xhigh | 1 | 453186 | 28784000 | 150581 | 71341 | 29387767 |
| gpt-6-sol | medium | 1 | 114922 | 5152384 | 27316 | 5110 | 5294622 |

Captured UTC: 2026-09-22T20:14:59.5678193Z

See token-usage.csv for per-session roles, source paths and each counter timestamp. Refresh next turn to include the coordinator final response.
