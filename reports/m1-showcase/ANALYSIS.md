# Reading the M1 token graphs

The page uses the frozen start, M0, and M1 checkpoint snapshots in `benchmarks/milestones/`. It follows the [GPT-6 Sol and Luna article's](https://openai.com/index/introducing-gpt-6-sol-and-luna/) restrained black editorial layout and fine chart marks without using its benchmark data or branding.

## What the record shows

- The three M0/M1 Sol roles recorded **25,880,941 input tokens** between the start baseline and the pinned M1 checkpoint. **25,423,104 (98.23%)** were cached input; **457,837** were fresh input.
- They recorded **120,686 output tokens**, of which **49,307** were reasoning output. Reasoning is already included in output, just as cached input is already included in total input.
- The coordinator (Sol xhigh) accounted for 19,021,663 input tokens, but only 227,423 of those were fresh. The builder (Sol medium) used 114,922 fresh input; the contract reviewer (Sol high) used 115,492.
- Fresh input rose from **215,390** during the start-to-M0 interval to **242,447** during M0-to-M1. Output was **61,122** and **59,564** respectively. These are checkpoint windows, not comparable experiments: the builder was already active at the M0 checkpoint and the work differed by phase.

The engineering result is a **32-body provisional JNSQ Reborn Real catalog** with both Principia branches checked and **24/24 focused tests passing**. The catalog is still `analysis_ready=false`. A loaded KSP/Principia save, resolved ModuleManager configuration, complete Cartesian states, frame, epoch, and force-model evidence remain necessary before trajectory analysis or a Principia-equivalence claim.

This record supports an observation about cache reuse and role allocation. It does **not** measure model intelligence, cost per task, subscription consumption, or billing. There was no Luna M0/M1 run and no controlled comparison with Astra. The pinned M1 checkpoint precedes the coordinator's final response; the current rolling ledger includes later activity and is intentionally not substituted into this page.

## Account usage shown separately

The Codex account's seven-day usage window was **66% used and 34% remaining** when checked at **22 September 2026, 20:00:56 UTC**. The reported reset is **26 September 2026, 16:18:06 UTC**. This is account-wide usage, so it includes work outside this KSP project and cannot be calculated from the token rows. The page displays the capture time because the percentage will change.

## Files

- `index.html`: offline, screenshot-ready page; open directly in a desktop browser.
- `screenshot.png`: 1600 × 900 share image matching the editorial page layout. It is rendered from the same frozen data without opening the local HTML in a browser.
- `recorded-input.svg` and `milestone-increments.svg`: separate scalable graphs.
- `account-usage.json`: the separate timestamped account usage reading.
- `data.json`: the exact values derived from the checkpoint snapshots, plus the distinct account usage snapshot.
- `scripts/build-m1-showcase.mjs`: rebuilds the page and SVGs with installed Node.js, without packages. `scripts/render-m1-share-image.py` refreshes the share image using Pillow bundled with this workspace; no package was installed for this report.
