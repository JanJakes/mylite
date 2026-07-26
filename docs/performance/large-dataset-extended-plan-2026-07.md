# Extended Large-Dataset Performance Plan, July 2026

## Objective

Extend the deterministic MyLite-versus-bundled-SQLite qualification beyond the
initial read/write matrix. The extended suite must exercise selectivity,
temporary work, join topology, write amplification, foreign-key fan-out,
WordPress-shaped access, concurrency, data skew, result transfer, and database
lifecycle behavior at 100K, 500K, and 1M fact rows.

The suite is diagnostic rather than a universal engine score. Every comparison
must use equivalent logical data and operations, consume all returned values,
and fail on result, affected-row, or checksum differences.

## Measurement contracts

1. **Retained statement:** prepare once, then reset, bind, step, and consume.
2. **Prepare each:** include normalization, MySQL parsing, planning, lowering,
   SQLite preparation, execution, and result conversion.
3. **Write rollback:** execute retained writes in an explicit transaction and
   roll back outside the timed region.
4. **Concurrent wall time:** start independent handles together, measure all
   worker completion, and report aggregate operations and errors.
5. **Lifecycle wall time:** measure open/reopen, maintenance, and storage
   transitions separately from query execution.
6. **PHP/WordPress boundary:** measure mysqli/`wpdb` query construction,
   extension conversion, and PHP result materialization separately from the
   native C comparator.

Warm and cold-cache results are distinct contracts. No result from a shared
virtualized host is a cross-machine absolute baseline.

## Scenario families

| Area | Required axes |
| --- | --- |
| Selectivity and indexes | 0, 1, 0.01%, 1%, 10%, and 100%; covering/non-covering; NULL; OR; large IN; deep OFFSET |
| Sort and temporary work | unindexed top-N; full high-cardinality grouping and DISTINCT; window partition/order |
| Joins | three-table chain; star/fan-out; LEFT JOIN; anti-join; large-large bounded join; skewed join |
| Writes | multi-row batches; 0/1/5/10 indexes; UPSERT hit/miss; selective UPDATE and DELETE |
| Foreign keys | composite lookup; valid/invalid child insert; RESTRICT; SET NULL; cascading fan-out |
| WordPress | posts/postmeta lookup; meta filtering; taxonomy join; pagination and found rows; PHP/`wpdb` |
| Concurrency | parallel readers; readers plus rollback writer; long reader versus writer |
| Statistics and skew | hot/cold keys under deterministic skew; before/after ANALYZE |
| Result transfer | aggregate-only, narrow, wide, and 100/10K/large row streams; cursor, buffered, and PHP |
| Lifecycle and storage | warm/cold reopen; mass delete rollback/commit; fragmentation; optimize/reclaim |

## Dataset policy

- Core data remains deterministic and uses stable integer/text generation.
- Additional columns encode NULL and skew without random generators.
- Foreign-key fan-out data is bounded independently from the main fact count so
  smoke tests remain fast.
- WordPress-shaped tables are seeded only by the WordPress suite.
- Explicit database base paths support seed-once/reuse-many qualification.
- Dataset verification checks schema marker, requested cardinality, and all
  table counts before reuse.

## Acceptance criteria

- Every smoke scenario runs for both engines at 1,000 rows.
- Native result samples match operation count, row count, value bytes, affected
  rows, and checksum.
- Concurrent workloads finish with zero errors and deterministic operation
  totals.
- Lifecycle operations leave both databases readable and count-equivalent.
- Release, formatting, clang-tidy, and targeted ASan/UBSan checks pass.
- A repeated ratio of at least 2x with at least 50 microseconds absolute
  difference, or a non-linear slope, receives focused profiling.
- Unsupported constructs are reported as compatibility gaps, not silently
  removed from the matrix.

## Qualification sequence

1. Run all scenarios at 1,000 rows as a correctness smoke.
2. Seed reusable 100K, 500K, and 1M database pairs.
3. Run focused scenario families against each pair with alternating engine
   order and at least three samples.
4. Profile repeated material gaps in the profiling build.
5. Implement only evidence-backed optimizations.
6. Rerun affected scenarios and record final measurements and limitations.

## Completion

Completed on 2026-07-26. The implementation covers all ten families with 57
native scenarios, five system/concurrency scenarios, and seven real-`wpdb`/PHP
scenarios. It was qualified at 100K, 500K, and 1M fact rows; 10M-scale work was
explicitly deferred. The measurements, optimizations, residual costs, and
verification evidence are recorded in
[large-dataset-extended-qualification-2026-07.md](large-dataset-extended-qualification-2026-07.md).
