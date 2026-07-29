# Retained-Write Attribution Smoke, July 2026

## Status

Preliminary 100K evidence only. This is not a qualification result and does
not authorize an optimization.

The four-layer harness completed five balanced timing samples and five counter
samples at 100K rows with identical final state. Required Release 100K/1M runs,
published raw artifacts, and sampled call graphs remain pending.

## Environment and Method

The run used clean revision `6f63f0ce1ebf092a3f171b84f5fa4a3a1b0b3183`
on Linux 6.12.96, x86-64, pinned to logical CPU 0. Databases were fresh per
layer. Filesystem and executable caches were not dropped, and forward/reverse
layer order rotated between samples.

The timing client was a profiling-disabled Development build. The counter
client was a profiling-enabled Release build. Counter timings are intentionally
excluded from wall-time attribution because statement-status and callback
instrumentation perturb execution.

All layers produced dataset hash `1948992273031130503` after 403,399 logical
writes. Exact generated programs, physical names, parameter counts, all 13
table checksums, foreign-key state, affected rows, and reopen verification
matched.

## Preliminary Timing

| Layer | Median | MAD | Delta | Residual share |
| --- | ---: | ---: | ---: | ---: |
| Native SQLite | 4,242.601 ms | 5.575% | baseline | — |
| MyLite physical schema | 4,740.531 ms | 0.425% | 497.930 ms | 2.662% |
| Generated guarded SQL | 14,697.433 ms | 2.074% | 9,956.902 ms | 53.235% |
| Full MyLite | 22,946.299 ms | 1.362% | 8,248.866 ms | 44.103% |

The full/native ratio was 5.409x and the absolute residual was 18,703.698 ms.
The five total-time samples were:

| Layer | Samples (ms) |
| --- | --- |
| Native SQLite | 4639.160, 4242.134, 4513.130, 4242.601, 4006.080 |
| MyLite physical schema | 4720.407, 4813.144, 5627.178, 4738.717, 4740.531 |
| Generated guarded SQL | 14374.728, 14939.494, 16872.266, 14697.433, 14392.566 |
| Full MyLite | 22946.299, 23258.849, 22408.751, 23163.380, 22601.097 |

Layer deltas telescope to the complete residual by construction. They locate
where cost enters the stack but do not independently prove which functions or
data structures consume it.

## Counter Evidence

The table reports medians from the separate instrumented run:

| Layer | SQLite steps | VM steps | Metadata VM | Collation callbacks | Collation time | MyLite allocations |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Native SQLite | 403,399 | 18,178,669 | 0 | 0 | 0 ms | 0 |
| MyLite physical schema | 403,399 | 15,375,676 | 0 | 2,376,417 | 135.307 ms | 0 |
| Generated guarded SQL | 403,399 | 23,777,755 | 0 | 2,376,417 | 145.156 ms | 0 |
| Full MyLite | 1,210,495 | 27,016,637 | 11,690 | 2,376,417 | 150.079 ms | 298 |

Generated guards add 8,402,079 VM steps over the physical-schema layer. The
public layer adds 3,238,882 VM steps and records 807,096 more SQLite step calls
than the guarded direct layer. It also records 11 retained DML plans, 403,388
plan hits, 1,210,184 statement-cache hits, 13 misses, and 3,044,526 bytes
across 298 instrumented allocations.

No generated scalar callbacks executed. Custom collation callback count is
identical across the three MyLite-backed layers and measured callback time is
about 150 ms, far below either major layer delta. These counters make generated
guard work and public execution lifecycle the two candidates that require
call-graph attribution.

## Qualification Blocker

The host has no `/usr/bin/perf`, and `kernel.perf_event_paranoid` is `3`.
The runner wrote an explicit blocked profile record for all four layers and
failed. It did not produce a qualification summary.

The next valid run must use the Release timing default, cover both 100K and 1M
rows with at least five samples, publish its complete artifact directory, and
produce symbolized call graphs for every layer. Only then can the 1M residual
be explained quantitatively and an avoidable dominant cost be selected for
paired pre/post optimization.
