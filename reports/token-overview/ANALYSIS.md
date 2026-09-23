# Reading the current token overview

Captured at **23 September 2026, 09:49:44 UTC** from the registered
project sessions. The page keeps the restrained black editorial style of the
frozen M1 showcase, but reads the rolling benchmark ledger. It does not
overwrite the historical M1 checkpoint.

- **478,193,933 recorded input tokens:** 472,256,896 cached (98.8%) and
  5,937,037 fresh. Cached input is already part of total input.
- **1,470,854 output tokens**, including 545,706 reasoning tokens.
  Reasoning is already part of output.
- Sol xhigh accounts for 62.0% of recorded input and 52.6% of fresh input;
  Sol medium accounts for 29.0% and 26.8%, respectively. The chart uses
  actual model and reasoning-effort attribution from the task logs.
- The lower chart shows cumulative fresh input and output after the first
  benchmark baseline. Planning tokens before that baseline remain included
  in the model totals above. Milestone intervals contain different work, so
  this is not a controlled model comparison.

The separate Codex **account-wide** seven-day window was **74% used,
26% remaining** when checked at **09:50:05 UTC** on 23 September.
The reported reset is **26 September 2026, 16:18:06 UTC**. That percentage
includes other Codex work and cannot be derived from these project tokens.
The recorded counters do not establish billing or subscription consumption.

`index.html` is the offline page; `screenshot.png` is a 1600 × 900
headless Edge capture visually inspected at that size. The two SVG files are
the scalable graphs, `data.json` holds exact values, and
`account-usage.json` preserves the separate timestamped account reading.
Run `scripts/build-token-overview.py` with Python 3 to regenerate the page
from the then-current ledger. Regeneration needs a fresh account reading
if the displayed account percentage should also be current.
