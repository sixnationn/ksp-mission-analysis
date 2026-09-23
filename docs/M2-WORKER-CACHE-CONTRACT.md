# M2 mission-worker cache integration contract

Specified on 23 September 2026 before worker implementation. The `start_mission` worker already validates a complete synthetic or exact-byte runtime snapshot and integrates an independent Newtonian planetary ephemeris. This slice lets an explicit caller path reuse that validated ephemeris across mission runs. The legacy single-leg `start` command and fixed-route evaluator remain unchanged.

## Request and state transitions

An optional top-level `ephemeris_cache` object contains exactly one nonempty `path` string. Without it, `start_mission` keeps the current in-memory behavior and event schema. With it, the worker validates the source, frame, epoch, ephemeris settings, role atmospheres, work limits and route preflight before any cache or search work. It then emits `started` and a cache progress phase. If the path names an existing regular file, it calls `load_ephemeris_cache(path, snapshot, settings)` and uses only those samples. If absent, it calls `integrate`, then `save_ephemeris_cache` to publish the complete result before search. Existing invalid, incompatible, inaccessible, or changing cache files are errors, never silent misses or reasons to recompute. A simultaneous creator that wins the path is an explicit write failure; the worker never overwrites it. An unwritable path fails without emitting a route or successful completion.

Cache-enabled terminal events disclose `ephemeris_cache_status` as `hit` or `created` after successful load/save; cache failures use a stable error code `cache_rejected` or `cache_write_failed`. Every emitted event retains the original `snapshot_hash` and `source_confidence`, and the final ephemeris metadata remains unchanged. Neither a cache hit nor a file checksum promotes `synthetic_fixture` or `runtime_observed_uncompared` to installed KSP/Principia validation. Cancellation before cache work exits without creating a file; cancellation after a successful save may leave the valid immutable file, and reports no completed route if search has not completed. Cache load/save are bounded noninterruptible stages.

## Observable failure cases before implementation

| Case | Required observation |
|---|---|
| No cache request | Same deterministic events and route results as before. |
| Valid synthetic mission, fresh unique cache path, then same request/path | First run reports `created`; second reports `hit`; source envelope, ephemeris metadata, route IDs and ranked values agree. The file exists and is unchanged on hit. |
| Existing file changed, truncated, wrong format, stale snapshot/body physics/frame/epoch/step/coverage/fit settings | `cache_rejected`, no route or `complete`, file unchanged. No hidden reintegration. |
| Missing parent, unwritable destination, or competing creator | `cache_write_failed`, no route or `complete`; no partial destination. |
| Missing/empty/malformed cache request path or unsupported cache object field | `invalid_request` before `started`. |
| Source hash/frame/epoch or runtime provenance invalid | Existing source rejection before cache work, even if a cache file exists. |
| Cancellation before cache work | `cancelled`, no cache creation or route. |

Acceptance uses the existing worker tests with a unique disposable directory, local MSYS2 and MSVC builds, and Ubuntu/Windows CI. Results remain synthetic screening unless a separate installed KSP/Principia comparison is recorded.
