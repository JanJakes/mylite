# Large-Dataset Performance Qualification, July 2026

## Purpose

This qualification compares MyLite end to end with the exact bundled SQLite
engine over deterministic datasets containing hundreds of thousands to millions
of rows. It is intended to find scaling defects and material compatibility-layer
overhead in reads, joins, expressions, foreign keys, and writes.

The benchmark is a diagnostic and qualification tool, not a universal
MyLite-versus-SQLite score. MyLite performs MySQL parsing, catalog analysis,
semantic validation, SQL lowering, metadata maintenance, and MySQL-compatible
foreign-key handling that direct SQLite does not. Every reported ratio must
therefore be interpreted with its absolute latency, work performed, and runtime
profile.

## Dataset

`mylite_large_dataset_benchmark` creates separate MyLite and SQLite files with
equivalent logical schemas and deterministic values:

- `accounts`: one parent row per 100 fact rows, with an indexed region.
- `items`: the requested fact-row count, with a primary key, parent foreign
  key, integer metric, status, timestamp, title, payload, and four secondary
  indexes.
- `item_tags`: one bridge row per fact row, with a composite primary key,
  secondary tag index, and cascading foreign key.
- `write_log`: initially empty, with an indexed foreign key to `items`.

At `--rows 1000000`, the two large tables contain one million rows each and the
dataset contains just over two million logical rows in total.

Both databases use the bundled SQLite implementation, DELETE journaling, normal
file-backed operation, explicit transactions for seeding, and enabled logical
foreign-key enforcement. MyLite enforces foreign keys through its compatibility
layer; direct SQLite uses its native foreign-key engine.

## Measurement contract

The suite reports three execution contracts:

- `prepare_each`: prepare, bind, step, consume, and finalize on every operation.
  This compares MyLite parsing/planning/lowering with direct SQLite preparation.
- `prepared`: prepare once outside measurement, then reset, bind, step, and
  consume on every operation.
- `write_rollback`: retain a prepared write statement, execute the measured
  operations inside one explicit transaction, and roll back after timing.
  Transaction start and rollback are outside the timed region, isolating
  statement and constraint-processing CPU cost from durable sync latency.

Each sample warms both engines first. Engine order alternates by sample to
reduce order and thermal bias. Every result cell is consumed and hashed.
MyLite and SQLite must produce identical operation counts, result-row counts,
value-byte counts, affected rows, and hashes for every sample or the benchmark
fails.

Samples report raw timing rows. Summary rows report median average operation
time and the MyLite/SQLite ratio. Seeding phase throughput and final database
file sizes are reported separately.

## Scenario matrix

| Scenario | Construct |
| --- | --- |
| `point_lookup_prepare_each` | Primary-key lookup including preparation |
| `point_lookup_prepared` | Retained primary-key lookup |
| `secondary_lookup` | Secondary index, ordered top 20 |
| `range_aggregate` | Composite range index with count and sum |
| `full_scan_expression` | Million-row arithmetic predicate and aggregates |
| `text_expression` | Indexed text predicate, `LIKE`, scalar projection, ordered streaming |
| `group_aggregate` | Full grouped aggregate over 1,000 groups |
| `indexed_order_limit` | Composite text/timestamp index with top 100 |
| `parent_join` | Indexed parent-to-fact join and grouped aggregate |
| `bridge_join` | Indexed bridge lookup, fact join, scalar arithmetic, ordered streaming |
| `correlated_exists` | Correlated `EXISTS` using the parent/metric index |
| `indexed_update_rollback` | Primary-key update with secondary-index maintenance |
| `foreign_key_insert_rollback` | Valid child insert with FK validation |
| `foreign_key_cascade_rollback` | One-level cascading delete |

The suite deliberately uses an integer metric. During harness development,
MyLite rejected `SUM()` over a `DOUBLE` descriptor. It also rejected a recursive
`accounts -> items -> item_tags` cascade. Those are compatibility limitations,
not performance measurements, and are recorded separately from the supported
scenario results.

## Investigation criteria

A scenario requires focused profiling when all of the following hold:

1. The median MyLite/SQLite ratio is at least 2x.
2. The median absolute difference is at least 50 microseconds per operation, or
   the difference materially affects bulk throughput.
3. The gap repeats across at least two dataset sizes or has a clear scaling
   slope.

Ratios below this threshold can still matter for high-frequency point
operations. Ratios above it are not automatically defects when they represent
required MySQL-compatible work. Profiling must distinguish MyLite frontend,
catalog, constraint, result-consumption, and SQLite execution time before an
optimization is accepted.

## Commands

Build and smoke-test the harness:

```sh
cmake --preset ci
cmake --build build/ci --target mylite_large_dataset_benchmark_check
```

Run one full qualification size:

```sh
build/ci/packages/libmylite/mylite_large_dataset_benchmark \
  --rows 100000 \
  --samples 5 \
  --warmup 1 \
  --database-dir build/perf-data \
  --output build/perf-data/large-dataset-100000.csv
```

Use `--scenario NAME` for focused runs, `--iterations N` to override a
scenario's default, `--list` to inspect defaults, and `--keep-databases` when
query plans or file contents need inspection.

## Qualification environment

The final qualification was run on 2026-07-26 from the worktree based on
`432ebdbb0c8af07620148d68a602137f82547ec8`:

- Linux 6.12.96, x86-64, Debian 13.
- 18-vCPU KVM guest backed by an AMD EPYC processor.
- GCC 14.2.0, CMake 3.31.6, `Release`, profiling disabled.
- ext2/ext3-family block-backed filesystem with 4 KiB blocks.
- CPU affinity fixed to logical CPU 8 with `taskset`.
- Five measured samples per scenario, one warmup iteration, median summaries.

This is a shared virtualized host rather than a frequency-controlled benchmark
machine. The absolute times should not be compared with unrelated machines.
The paired engine order, repeated samples, identical bundled SQLite, and
result-equivalence checks make the within-run ratios useful for identifying
MyLite overhead and scaling behavior.

## Results

All 420 scenario/engine samples passed result equivalence. No scenario was
discarded.

### Read, expression, join, and write ratios

The following table reports the median MyLite/SQLite time ratio. Lower is
better; `1.0x` is parity.

| Scenario | 100K | 500K | 1M |
| --- | ---: | ---: | ---: |
| Primary-key lookup, prepare each | 3.01x | 2.80x | 2.93x |
| Primary-key lookup, retained prepare | 1.39x | 1.47x | 1.46x |
| Secondary-index lookup | 1.59x | 1.51x | 1.44x |
| Composite indexed range aggregate | 2.19x | 1.50x | 1.30x |
| Full-scan arithmetic expression | 1.56x | 1.60x | 1.57x |
| Text predicate and projection | 1.89x | 1.91x | 1.85x |
| Grouped aggregate | 1.20x | 1.10x | 0.98x |
| Indexed order and limit | 2.12x | 2.23x | 2.12x |
| Parent join and aggregate | 0.94x | 1.03x | 1.10x |
| Bridge join and projection | 1.50x | 1.18x | 1.08x |
| Correlated indexed `EXISTS` | 1.25x | 1.11x | 1.11x |
| Indexed update with rollback | 1.44x | 1.61x | 1.54x |
| Valid FK child insert | 15.33x | 7.32x | 9.19x |
| Cascading parent delete | 2.61x | 2.02x | 2.29x |

The cascade row uses the final post-optimization focused rerun with seven
samples and 20 deletes per sample. All other rows use the five-sample full
matrix.

The one-million-row absolute medians show which ratios are material:

| Scenario | MyLite | SQLite | Difference |
| --- | ---: | ---: | ---: |
| Primary-key lookup, prepare each | 85.4 us | 29.2 us | 56.3 us |
| Primary-key lookup, retained prepare | 28.0 us | 19.1 us | 8.8 us |
| Secondary-index lookup | 34.3 us | 23.9 us | 10.4 us |
| Composite indexed range aggregate | 131.5 us | 100.8 us | 30.7 us |
| Full-scan arithmetic expression | 141.4 ms | 90.3 ms | 51.2 ms |
| Text predicate and projection | 490.3 ms | 265.2 ms | 225.1 ms |
| Grouped aggregate | 139.9 ms | 142.6 ms | -2.7 ms |
| Indexed order and limit | 105.4 us | 49.8 us | 55.6 us |
| Parent join and aggregate | 6.0 ms | 5.5 ms | 0.5 ms |
| Bridge join and projection | 39.7 ms | 36.7 ms | 3.0 ms |
| Correlated indexed `EXISTS` | 21.5 ms | 19.3 ms | 2.1 ms |
| Indexed update with rollback | 189.5 us | 123.3 us | 66.2 us |
| Valid FK child insert | 54.4 us | 5.9 us | 48.5 us |
| Cascading parent delete | 285.6 us | 124.6 us | 161.0 us |

The retained primary-key lookup isolates SQLite statement execution from most
MyLite frontend work. Its 1.46x ratio and 8.8 us absolute difference are much
smaller than the prepare-each result, confirming that normalization, MySQL
parse/analyze/lower work, and catalog synchronization dominate short
prepare-each operations.

The grouped aggregate, parent join, bridge join, and correlated `EXISTS` results
show no material MyLite query-plan regression at scale. Their ratios move
toward parity as row work dominates fixed compatibility overhead. Full scans
and text expressions remain slower, but their slopes are linear rather than
superlinear.

### Load throughput

Seeding uses retained single-row statements inside one transaction. It
therefore measures CPU and compatibility work, not one durable sync per row.

| Fact rows | Logical rows inserted | MyLite | SQLite | MyLite avg/row | Ratio |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 100,000 | 201,000 | 18.25 s | 1.63 s | 90.8 us | 11.17x |
| 500,000 | 1,005,000 | 88.77 s | 12.84 s | 88.3 us | 6.91x |
| 1,000,000 | 2,010,000 | 183.54 s | 27.00 s | 91.3 us | 6.80x |

MyLite's nearly constant 88-91 us per inserted row demonstrates linear
scaling after the fixes below. The remaining 6.8x bulk-load gap is significant:
MyLite still plans and validates MySQL-compatible DML for every retained
statement execution, while direct SQLite resets a native VM and relies on its
native constraint engine.

### Database size

| Fact rows | MyLite file | SQLite file | MyLite overhead |
| ---: | ---: | ---: | ---: |
| 100,000 | 25,649,152 B | 25,530,368 B | 118,784 B |
| 500,000 | 129,933,312 B | 129,814,528 B | 118,784 B |
| 1,000,000 | 260,071,424 B | 259,952,640 B | 118,784 B |

The overhead is fixed catalog storage, not per-row amplification. At one
million fact rows the MyLite file is only 0.046% larger.

### Focused phase profile

An instrumented 100K-row, one-sample run was used for attribution only:

- Full-scan arithmetic spent 43.8 of 44.3 ms in SQLite stepping. MyLite's
  compatibility arithmetic executes inside the SQLite VM, so there is no
  separate planner or result-buffer scaling defect to remove.
- The text scenario spent 190.5 of 362.1 ms in SQLite stepping and 325.3 ms in
  inclusive cursor stepping while returning about 70,000 rows across three
  iterations. MySQL-compatible text comparison plus result conversion is the
  largest remaining read-side opportunity.
- Grouping spent 35.4 of 39.1 ms in SQLite stepping, consistent with the
  near-parity Release result.
- Five hundred indexed updates used 4.6 ms for catalog steps and 50.8 ms for
  all SQLite steps after metadata reuse. Five hundred valid FK inserts used
  6.1 ms for catalog steps and 10.5 ms for all SQLite steps.
- Five cascading deletes took 1.6 ms total, of which 1.0 ms was SQLite
  stepping. This is no longer dependent on total child-table size.

Profile timers add clock overhead and overlap at inclusive boundaries. They are
not substituted for the uninstrumented Release measurements.

## Findings and follow-up

The qualification found and corrected three material runtime problems:

1. **Child inserts were quadratic.** Every successful child insert performed a
   post-write scan of the complete child table. A 100K-row setup did not finish
   after more than three minutes. INSERT and LOAD DATA now validate each planned
   child tuple through the parent key after writing it. The full-table fallback
   remains only for `ON DUPLICATE KEY UPDATE`, whose final values are not
   represented by the original insert tuple.
2. **Cascades scanned the child table and then globally revalidated it.**
   DELETE actions now select affected parent keys and use the generated child
   FK index. Non-self-referencing restrictions use the same targeted probe;
   only self-referencing deletes retain post-write validation for multi-row
   semantics. The 100K-row cascade median fell from 54.0 ms to 0.32 ms, about a
   170x MyLite improvement.
3. **DML repeatedly rebuilt immutable key metadata.** INSERT, INSERT SET,
   INSERT SELECT, LOAD DATA, and UPDATE now borrow the existing
   generation-safe table key metadata. UPDATE first probes whether an assigned
   column participates in an FK and loads complete FK descriptors only when
   needed. The focused unrelated-update profile fell from about 121 us to
   50 us per operation before final file-backed qualification.

Regression coverage traces warmed FK writes and rejects both global validation
scans and repeated table-index catalog scans. Existing FK, INSERT variants,
INSERT SELECT, LOAD DATA, and benchmark result-equivalence suites cover the
changed ownership and behavior.

### Remaining opportunities

- **Retained DML planning:** Bulk load and tiny FK-write throughput remain the
  clearest gaps. A reusable, generation-invalidated native DML plan would avoid
  rebuilding MyLite planning state on each `mylite_stmt_step()`. This is an
  architectural optimization and must preserve parameter-dependent conversion,
  diagnostics, SQL modes, schema generation invalidation, and session state.
- **Text execution and row conversion:** The 1M text scenario is 1.85x SQLite
  and 225 ms slower while returning many rows. Further work should separately
  benchmark MySQL `LIKE`/collation callbacks and cursor-to-public-value
  conversion before changing either.
- **Short prepare-each operations:** A primary-key lookup that includes MyLite
  preparation remains about 56 us slower. This is expected compatibility
  frontend work, but a safely scoped analyzed-plan cache could help workloads
  that cannot retain statements.
- **FK fixed cost:** Valid FK inserts and cascades still have high ratios
  because direct SQLite completes them in a few to hundreds of microseconds.
  Their absolute MyLite differences are approximately 49 us and 161 us at one
  million rows, with no growth tied to total table cardinality.

No supported scenario in this matrix retains a MyLite-specific algorithmic
scaling defect. The remaining gaps are fixed compatibility overhead or
linear per-row expression/result work. Future optimization should prioritize
retained DML plans and text/result conversion based on application profiles,
not the ratio of isolated micro-operations alone.
