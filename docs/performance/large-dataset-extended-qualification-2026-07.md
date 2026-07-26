# Extended Large-Dataset Qualification, July 2026

## Status

Complete for 100K, 500K, and 1M fact rows. Work at 10M or more rows was
explicitly deferred.

The qualification covers all ten requested areas:

1. selectivity and index behavior;
2. sorting and temporary work;
3. join topology;
4. write scaling and index amplification;
5. foreign-key fan-out;
6. WordPress-shaped access through real `wpdb` and mysqli;
7. concurrent readers and writers;
8. statistics and deterministic skew;
9. native and PHP result transfer;
10. reopen, delete, fragmentation, and storage reclamation.

It found no superlinear MyLite query-execution defect through 1M fact rows. It
did find two material hot-path costs and one false maintenance implementation.
All three were corrected and remeasured.

## Environment

The final runs used the worktree based on
`3151ccccb3b7723fad300a31d88dfb7aeda1ede5`:

- Linux 6.12.96, x86-64, Debian 13;
- AMD EPYC KVM guest with 18 logical CPUs;
- Clang 19.1.7, CMake 3.31.6, `Release`;
- ext4 storage with 4 KiB blocks;
- local file-backed databases;
- native measurements pinned to logical CPU 8 where the harness supported it.

This is a shared virtual machine, not a frequency-controlled benchmark host.
Absolute timings must not be compared with unrelated machines. Paired
MyLite/SQLite measurements, repeated samples, alternating engine order, and
result checks make the within-run comparisons useful.

## Implemented Suites

### Native core suite

`mylite_large_dataset_benchmark` now has 57 scenarios. It supports:

- deterministic seed-once fixtures through `--database-base` and `--seed-only`;
- verified reuse through `--reuse-databases`;
- focused selection through `--scenario`;
- real statistics refresh through `--analyze`;
- retained-prepare, prepare-each, write-rollback, and expected-error contracts;
- per-sample result rows, value bytes, affected rows, checksums, timing, and
  paired ratios.

The expanded 1M fixture performs 4,030,399 logical seed operations. It includes
one million `items`, one million bridge rows, 10,000 accounts, deterministic
skew and NULLs, insert targets with 0/1/5/10 indexes, composite foreign keys,
and CASCADE, RESTRICT, and SET NULL fan-out tables.

### System suite

`mylite_large_dataset_system_benchmark` opens independent handles against a
retained pair and measures:

- repeated reopen plus `COUNT(*)`;
- four concurrent readers;
- four readers plus one rollback writer;
- a long reader overlapping a writer;
- committed mass delete followed by equivalent vacuum and analyze work on
  disposable copies.

Worker operation counts and checksums are deterministic. Any worker error fails
the run.

### WordPress/PHP suite

`wordpress_large_dataset.php` loads the pinned real WordPress
`class-wpdb.php`. It uses only minimal hook stubs needed to instantiate `wpdb`;
query construction, escaping, mysqli calls, result creation, and `wpdb`
materialization are the real WordPress paths.

At 1M posts the fixture contains:

- 1,000,000 `wp_posts` rows;
- 2,000,000 `wp_postmeta` rows;
- 1,000,000 `wp_term_relationships` rows.

The scenarios cover prepared point lookup, `SQL_CALC_FOUND_ROWS` pagination,
indexed postmeta filtering, taxonomy joins, 10K-row `wpdb` materialization,
buffered mysqli, and streaming mysqli.

### Automated smoke

`mylite_large_dataset_benchmark_check` runs all 57 native scenarios for both
engines at 1,000 rows, then runs the system suite against a retained pair. The
Ubuntu Clang CI job executes this target. The WordPress CI job runs a separate
1,000-post real-`wpdb` smoke and uploads its CSV output.

## Correctness Evidence

The final native scale pass produced 1,026 measured engine samples:

- 57 scenarios;
- two engines;
- three samples;
- three dataset sizes.

All paired read samples matched operation count, result-row count, value-byte
count, and checksum. Write samples matched affected-row or expected-error
contracts and restored their fixture state through rollback.

The qualified system pass completed 30 engine/scenario rows with zero worker
errors. Cross-engine counts and checksums matched. The PHP pass completed 105
samples. Checksums were stable within each scenario and size, and the buffered
and streaming mysqli result hashes matched exactly.

## Final Native Results

The table below uses the final 1M-row median. Ratios must be read together with
the absolute time. A high ratio on a 30-150 microsecond operation is fixed
compatibility overhead, not a scaling failure.

| Scenario | MyLite | SQLite | Ratio |
| --- | ---: | ---: | ---: |
| Point lookup, prepare each | 108.6 us | 18.2 us | 5.95x |
| Point lookup, retained prepare | 19.2 us | 12.8 us | 1.50x |
| Secondary-index lookup | 35.5 us | 23.9 us | 1.49x |
| Full arithmetic scan | 139.1 ms | 94.4 ms | 1.47x |
| Text predicate/projection | 337.9 ms | 293.7 ms | 1.15x |
| Grouped aggregate | 154.6 ms | 137.4 ms | 1.13x |
| Unindexed top-1000 sort | 240.7 ms | 193.9 ms | 1.24x |
| Three-table join | 198.2 ms | 193.6 ms | 1.02x |
| Bounded large-large join | 16.7 ms | 11.3 ms | 1.48x |
| Anti-join | 458.5 ms | 435.9 ms | 1.05x |
| Hot skewed key after analyze | 56.5 ms | 56.2 ms | 1.01x |
| 10% update plus rollback | 2.93 s | 2.69 s | 1.09x |
| 1% delete plus rollback | 749.4 ms | 770.8 ms | 0.97x |
| CASCADE fan-out plus rollback | 136.3 ms | 114.3 ms | 1.19x |
| SET NULL fan-out plus rollback | 94.5 ms | 70.4 ms | 1.34x |
| Narrow full-result stream, optimized | 209.9 ms | 200.8 ms | 1.05x |
| Wide 10K-result stream, optimized | 13.9 ms | 11.0 ms | 1.27x |

### Selectivity and indexes

0-row, 1-row, 0.01%, 1%, 10%, and full-selectivity cases all return correct
results. At 1M rows:

- 1% selectivity is 0.94x SQLite;
- 10% is 1.02x;
- full selectivity is 1.01x;
- covering and non-covering composite ranges are 249 and 203 microseconds;
- primary-key OR lookup is 29 microseconds;
- a 90%-deep offset is 46.2ms versus 43.2ms.

Zero, one-row, and cold-miss ratios range from 2.9x to 4.8x, but their MyLite
latency is only 46-95 microseconds. They expose fixed parse/plan/catalog work,
not growth with table size.

### Sorts, grouping, and windows

Unindexed sorting is linear and 1.24-1.26x SQLite from 100K through 1M after
the collation optimization. High-cardinality grouping moves from 2.01x at 100K
to 1.14x at 1M as fixed setup cost is amortized. High-cardinality DISTINCT is
at parity at 1M. The window scenario is 4.60ms versus 2.76ms; the absolute gap
is 1.84ms and no adverse size slope appears.

### Joins

Three-table, bridge, parent, LEFT, anti-, correlated-EXISTS, and bounded
large-large joins remain linear. The three-table join is at parity at 1M.
There is no evidence that MyLite changes SQLite into a worse asymptotic join
plan for the tested shapes.

### Writes and index amplification

The single-row insert cases have high ratios because every MyLite operation
still executes MySQL-compatible DML validation and diagnostics:

- 0 indexes: 72.5 us, 3.99x SQLite;
- 1 index: 45.7 us, 2.51x;
- 5 indexes: 76.9 us, 2.59x;
- 10 indexes: 101.0 us, 2.11x.

A ten-row statement costs 90.2 microseconds total, about 9 microseconds per
row. Batching therefore amortizes most fixed MyLite work. Large selective
updates and deletes are within 0.96-1.19x SQLite at 1M rows.

### Foreign keys

Valid, invalid, composite, RESTRICT, and fan-out cases all preserve their
expected success or error behavior. Invalid composite FK insertion is 150
microseconds in MyLite versus 3.4 microseconds in SQLite, a large ratio but a
small absolute operation. The bulk CASCADE and SET NULL cases are only
1.19-1.34x at 1M rows, confirming that no table-wide child scan remains.

## Load Throughput

Seeding uses retained one-row statements inside transactions. It deliberately
measures statement execution and compatibility work, not one durable sync per
row.

| Fact rows | Logical operations | MyLite | SQLite | MyLite/op | Ratio |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 100,000 | 403,399 | 31.78 s | 2.81 s | 78.8 us | 11.29x |
| 500,000 | 2,015,399 | 170.86 s | 22.11 s | 84.8 us | 7.73x |
| 1,000,000 | 4,030,399 | 357.31 s | 46.33 s | 88.7 us | 7.71x |

This is the largest remaining throughput gap. It is linear, not quadratic.
The ten-row insert result demonstrates that statement batching removes much of
it. A future reusable DML-plan design could reduce the remaining per-execution
parse/validation work, but it must retain schema-generation, session-state,
diagnostic, trigger, auto-increment, and FK correctness.

## Statistics and Skew

`ANALYZE TABLE` now performs real SQLite statistics collection. After analyze,
the deterministic hot-tenant aggregate is at parity at every scale:

| Rows | MyLite | SQLite | Ratio |
| ---: | ---: | ---: | ---: |
| 100K | 5.56 ms | 5.60 ms | 0.99x |
| 500K | 25.34 ms | 25.38 ms | 1.00x |
| 1M | 48.92 ms | 49.15 ms | 1.00x |

Cold misses remain 35-37 microseconds in MyLite versus 11-16 microseconds in
SQLite. The difference is fixed frontend and catalog work.

## Concurrency and Lifecycle

The qualified concurrency pass uses 100 iterations per worker:

| 1M-row scenario | MyLite | SQLite | Result |
| --- | ---: | ---: | --- |
| Four readers, 400 operations | 55.7 us/op | 41.6 us/op | 1.34x, zero errors |
| Four readers plus writer, 500 operations | 49.1 us/op | 30.5 us/op | 1.61x, zero errors |
| Long reader plus writer | 83.4 ms wall | 105.5 ms wall | zero errors |

Ratios varied on shorter runs because these operations are close to scheduler
noise. The 100-iteration pass shows bounded overhead and no scale-dependent
collapse.

Repeated reopen plus count costs:

| Rows | MyLite | SQLite | Difference |
| ---: | ---: | ---: | ---: |
| 100K | 10.45 ms | 1.24 ms | 9.21 ms |
| 500K | 19.19 ms | 5.80 ms | 13.38 ms |
| 1M | 28.85 ms | 9.63 ms | 19.22 ms |

MyLite validates catalog shape, catalog row invariants, physical tables,
columns, indexes, and the file preamble on open. That correctness gate is the
main intentional reopen cost. Removing it for speed would weaken corruption
detection; a future optimization needs a durable integrity-validation design,
not a skipped check.

At 1M rows, deleting 100K rows, committing, vacuuming, and analyzing takes
14.28s in MyLite and 13.75s in SQLite. Both files reclaim the same logical
space:

- MyLite: 391,434,240 to 339,968,000 bytes;
- SQLite: 391,315,456 to 339,849,216 bytes.

The 118,784-byte difference is fixed MyLite format/catalog overhead.

## WordPress/PHP Results

The table reports five-sample medians. The 10K-row scenarios use four
iterations per sample; the others use twenty.

| Scenario | 100K posts | 500K posts | 1M posts |
| --- | ---: | ---: | ---: |
| `wpdb` prepared point | 106 us | 91 us | 112 us |
| Pagination plus found rows | 10.76 ms | 46.47 ms | 94.01 ms |
| Indexed postmeta filter join | 292 us | 169 us | 153 us |
| Taxonomy join, 100 rows | 0.38 ms | 3.22 ms | 6.39 ms |
| `wpdb` materialize 10K | 22.43 ms | 20.69 ms | 20.34 ms |
| mysqli buffered 10K | 20.20 ms | 17.17 ms | 16.45 ms |
| mysqli streaming 10K | 19.29 ms | 19.82 ms | 18.23 ms |

Point, postmeta, and fixed-size result transfer are independent of total table
size. Pagination scales with the number of rows counted for
`SQL_CALC_FOUND_ROWS`; taxonomy scales with the indexed match population.
Streaming and buffered mysqli have similar CPU cost at 10K rows. Streaming
still avoids retaining the full native result when the caller consumes rows
incrementally.

## Optimizations Implemented

### ASCII collation fast path

The default MySQL collation previously decoded every ASCII byte through the
Unicode iterator for each comparison. A semantics-preserving ASCII path now
handles primary case folding and AS-CS tertiary case weights directly.
Non-ASCII input uses the unchanged Unicode implementation.

Focused 1M-row improvements:

- unindexed sort: 531.6ms to 245.6ms, then 240.7ms in the full final pass;
- text predicate/projection: 540.3ms to 340.0ms, then 337.9ms;
- WordPress pagination/found rows: about 326ms to 94ms.

Unicode accent, normalization, case-sensitive, binary, grouping, and ordering
tests remain green.

### Integer result formatting

Streaming integer cells used general-purpose `snprintf`. The hot path now uses
a bounded decimal formatter that handles zero, signs, `INT64_MIN`, and
`INT64_MAX` without undefined negation.

Focused 1M-row improvements:

- narrow full stream: 301.5ms to 209.9ms, ratio 1.55x to 1.05x;
- wide 10K stream: 18.4ms to 13.9ms, ratio 1.70x to 1.27x.

### Real table maintenance

`ANALYZE TABLE` and `OPTIMIZE TABLE` previously returned success-shaped rows
without applying maintenance. They now:

- analyze each resolved physical table;
- use a full-file `VACUUM` for the single-file equivalent of persistent table
  recreation during optimize;
- analyze after the optimize rewrite;
- retain MySQL-compatible result rows and implicit-commit behavior.

Tests verify statistics population, physical file shrinkage, row preservation,
reopen, and preamble preservation.

## Residual Opportunities

The qualification leaves three credible future optimization areas:

1. **Retained one-row DML:** 7.7x load throughput at 1M, but linear and strongly
   improved by batching. Native reusable DML plans are the architectural
   opportunity.
2. **Open integrity validation:** 9-19ms additional reopen cost. Any reduction
   needs a corruption-safe durable validation strategy.
3. **Fixed compatibility overhead:** prepare-each reads and small FK/error
   operations add tens to roughly 150 microseconds. Ratios are large only when
   SQLite itself completes in a few microseconds.

No additional algorithmic read, sort, join, bulk write, FK fan-out,
concurrency, statistics, result-transfer, or storage bottleneck was found
through 1M rows.

## Limitations

- OS page caches were warm for query timing.
- Write-rollback scenarios exclude commit synchronization; the lifecycle suite
  measures committed delete and maintenance separately.
- Concurrency is multi-handle and multithreaded in one process, not a
  distributed server workload.
- The PHP suite has no direct SQLite PHP control because its purpose is the
  MyLite mysqli/`wpdb` boundary. Equivalent native result transfer is compared
  in the C suite.
- Only the tested deterministic schema and data distributions are qualified.
- 10M and larger datasets remain unmeasured by explicit scope decision.
