# Token benchmark

Registered task sessions only; active response is incomplete until next refresh.

Input includes cached input. Reasoning is a subset of output. Total is input plus output; do not add subsets again. Uncached input includes any cache-write input; cache writes are also reported separately.

Recorded runtime counters, not an invoice or weekly-limit estimate. Model attribution follows turn_context. No timing or quality claim follows from token counts alone.

| Model | Effort | Sessions | Fresh input | Cached input | Output | Reasoning subset | Total |
|---|---|---:|---:|---:|---:|---:|---:|
| gpt-6-astra | medium | 1 | 206571 | 2700032 | 15499 | 852 | 2922102 |
| gpt-6-astra | high | 1 | 54319 | 326528 | 2325 | 175 | 383172 |
| gpt-6-sol | high | 3 | 346644 | 7946752 | 47771 | 22126 | 8341167 |
| gpt-6-sol | xhigh | 1 | 1727911 | 149018880 | 527335 | 233056 | 151274126 |
| gpt-6-sol | medium | 2 | 657752 | 56708352 | 269847 | 76236 | 57635951 |

Captured UTC: 2026-09-22T23:26:03.1794560Z

See token-usage.csv for per-session roles, source paths and each counter timestamp. Refresh next turn to include the coordinator final response.
