# Token benchmark

Registered task sessions only; active response is incomplete until next refresh.

Input includes cached input. Reasoning is a subset of output. Total is input plus output; do not add subsets again. Uncached input includes any cache-write input; cache writes are also reported separately.

Recorded runtime counters, not an invoice or weekly-limit estimate. Model attribution follows turn_context. No timing or quality claim follows from token counts alone.

| Model | Effort | Sessions | Fresh input | Cached input | Output | Reasoning subset | Total |
|---|---|---:|---:|---:|---:|---:|---:|
| gpt-6-astra | high | 1 | 54319 | 326528 | 2325 | 175 | 383172 |
| gpt-6-astra | medium | 1 | 206571 | 2700032 | 15499 | 852 | 2922102 |
| gpt-6-sol | high | 3 | 986207 | 39432832 | 118361 | 58913 | 40537400 |
| gpt-6-sol | max | 1 | 510929 | 23724288 | 80854 | 48261 | 24316071 |
| gpt-6-sol | medium | 2 | 1609697 | 137722880 | 523466 | 138163 | 139856043 |
| gpt-6-sol | xhigh | 1 | 3419903 | 304464128 | 868342 | 375700 | 308752373 |

Captured UTC: 2026-09-23T19:49:30.5274361Z

See token-usage.csv for per-session roles, source paths and each counter timestamp. Refresh next turn to include the coordinator final response.
