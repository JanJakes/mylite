# MyLite Benchmarks

MyLite performance benchmarks are explicit developer tools, not default tests.
Build them with a Release configuration when comparing timings:

```sh
cmake --preset ci
cmake --build --preset ci \
  --target mylite_benchmark mylite_parser_recovery_benchmark
```

Run the built-in WordPress-inspired parser and runtime scenarios:

```sh
build/ci/packages/libmylite/mylite_benchmark --iterations 1000
```

Run only lexer, parser, or runtime scenarios:

```sh
build/ci/packages/libmylite/mylite_benchmark --only lexer --iterations 1000
build/ci/packages/libmylite/mylite_benchmark --only parse --iterations 1000
build/ci/packages/libmylite/mylite_benchmark --only runtime --iterations 1000
```

List scenarios or select one scenario:

```sh
build/ci/packages/libmylite/mylite_benchmark --list
build/ci/packages/libmylite/mylite_benchmark \
  --only runtime \
  --scenario runtime.wp_frontend_request \
  --iterations 1000 \
  --samples 7 \
  --warmup 2
```

`--warmup` runs unmeasured runtime iterations after database setup and before
each sample. Runtime scenarios default to one warmup iteration. Use
`--warmup 0` to measure the initial state, including the insert branch of an
upsert; the default measures steady-state request behavior. `--samples`
reports every sample plus minimum, median, 95th-percentile, and maximum average
operation times. Request-shaped scenarios also report the p50, p95, p99, and
maximum latency of complete request iterations for every sample. Pinning a run
to an otherwise idle CPU core, where supported, reduces scheduler noise.
`--per-query` expands each selected request scenario into independently measured
`.queryN` scenarios.

The benchmark reports CSV rows:

```text
scenario,kind,iterations,queries,operations,ok,errors,tokens,bytes,total_ms,avg_us,ops_per_sec
```

`kind=parse` measures lexing plus parsing into a MyLite AST and then freeing
the parse result. `kind=lexer` measures tokenization only. `kind=execute`
measures `mylite_execute()` against a temporary file-backed MyLite database and
does not use PHP, WordPress, or the PHP extensions.

## Large-Dataset MyLite/SQLite Qualification

The opt-in large-dataset suites create equivalent deterministic MyLite and
bundled-SQLite databases, verify result hashes and write outcomes, and exercise
59 native scenarios across selectivity, scans, temporary work, join topology,
writes, bulk import, foreign-key fan-out, skew, result transfer, concurrency,
and storage lifecycle:

```sh
cmake --preset ci
cmake --build build/ci --target mylite_large_dataset_benchmark_check

build/ci/packages/libmylite/mylite_large_dataset_benchmark \
  --rows 100000 \
  --samples 5 \
  --warmup 1 \
  --database-base build/perf-data/dataset-100000 \
  --keep-databases \
  --output build/perf-data/large-dataset-100000.csv
```

Seed once and reuse the same verified database pair for focused runs:

```sh
build/ci/packages/libmylite/mylite_large_dataset_benchmark \
  --rows 1000000 \
  --database-base build/perf-data/dataset-1000000 \
  --seed-only

build/ci/packages/libmylite/mylite_large_dataset_benchmark \
  --rows 1000000 \
  --database-base build/perf-data/dataset-1000000 \
  --reuse-databases \
  --analyze \
  --scenario skew_hot_tenant \
  --samples 7 \
  --output build/perf-data/skew-hot-1000000.csv

build/ci/packages/libmylite/mylite_large_dataset_system_benchmark \
  --rows 1000000 \
  --database-base build/perf-data/dataset-1000000 \
  --iterations 100 \
  --output build/perf-data/system-1000000.csv

build/ci/packages/libmylite/mylite_large_dataset_benchmark \
  --rows 1000000 \
  --database-base build/perf-data/dataset-1000000 \
  --reuse-databases \
  --scenario load_data_five_indexes_rollback \
  --iterations 1000000 \
  --samples 3 \
  --output build/perf-data/load-data-five-indexes-1000000.csv
```

`--analyze` runs `ANALYZE TABLE` for MyLite and equivalent SQLite `ANALYZE`
commands before measurement. `--reuse-databases` verifies the requested
cardinality and support-table counts before running. The system benchmark
requires a retained pair and uses independent handles and disposable copies for
reopen, concurrency, delete, and reclaim scenarios.

The PHP boundary has a separate benchmark that loads the pinned real
`class-wpdb.php`, seeds WordPress-shaped posts, postmeta, and taxonomy tables,
and compares `wpdb`, buffered mysqli, and streaming mysqli result paths:

```sh
MYLITE_WORDPRESS_LARGE_DB_PATH="$PWD/build/perf-data/wp-100000.mylite" \
  tools/wordpress-phpunit-mysqli-mylite benchmark-large-run \
    --posts 100000 \
    --meta-per-post 2 \
    --samples 5 \
    --iterations 20 \
    --output /work/build/perf-data/wordpress-100000.csv
```

Use `benchmark-large` instead of `benchmark-large-run` when the WordPress and
extension environment already exists. Pass `--reuse` to measure an existing
verified fixture and `--seed-only` to prepare one without timing queries.

The smoke target uses 1,000 rows, executes all native scenarios for both
engines, and runs the system suite. CI also runs a 1,000-post real-`wpdb`
smoke. Use `--list` to inspect native scenario defaults. The initial
qualification is in
[large-dataset-qualification-2026-07.md](large-dataset-qualification-2026-07.md);
the expanded methodology and final 100K/500K/1M results are in
[large-dataset-extended-qualification-2026-07.md](large-dataset-extended-qualification-2026-07.md).
The repeated-write implementation and final import measurements are in
[repeated-write-import-qualification-2026-07.md](repeated-write-import-qualification-2026-07.md).

### Retained-write attribution

Build separate Release clients for wall-time and counter collection:

```sh
cmake --preset ci
cmake --build --preset ci --target mylite_large_dataset_benchmark
cmake --preset perf-profile
cmake --build --preset perf-profile --target mylite_large_dataset_benchmark
mkdir -p build/perf-data

tools/run-retained-write-attribution \
  --artifact-dir build/perf-data/retained-write-qualification
```

The runner requires a new artifact path and a clean worktree. Its qualification
defaults pin one allowed CPU, run five balanced samples at 100K and 1M rows,
collect a separate instrumented counter matrix, and sample each 100K layer with
`perf`. It records the revision, client hashes, platform, cache policy, commands,
raw CSV, profiles, and summary. A formal run fails if a layer is missing,
correctness or programs drift, profiles are unavailable, or a layer's total
timing has more than 10% median absolute deviation.

For a quick non-qualifying client smoke:

```sh
build/ci/packages/libmylite/mylite_large_dataset_benchmark \
  --attribution-timing \
  --rows 1000 \
  --samples 2 \
  --warmup 0 \
  --database-base build/perf-data/retained-write-timing-smoke \
  --output build/perf-data/retained-write-timing-smoke.csv

build/perf-profile/packages/libmylite/mylite_large_dataset_benchmark \
  --attribution-seed \
  --rows 1000 \
  --samples 2 \
  --warmup 0 \
  --database-base build/perf-data/retained-write-counter-smoke \
  --output build/perf-data/retained-write-counter-smoke.csv
```

Use `--attribution-layer sqlite`, `mylite_physical`, `mylite_guarded`, or
`mylite` to isolate one layer for profiling. Counter-mode elapsed time includes
instrumentation overhead and is not a substitute for the uninstrumented timing
matrix. The current preliminary evidence and profiling blocker are documented
in
[retained-write-attribution-smoke-2026-07.md](retained-write-attribution-smoke-2026-07.md).

`runtime.wp_frontend_request` executes six representative WordPress frontend
reads per iteration. `runtime.wp_medium_frontend_request` uses nine WordPress
tables and eleven queries, including pagination with `SQL_CALC_FOUND_ROWS`,
taxonomy joins, comments, users, and metadata. `runtime.wp_write_request`
executes a five-query mixed request containing a read, an upsert, two updates,
and a delete. These scenarios measure a request-shaped sequence while retaining
the existing `.queryN` isolation for hotspot analysis.

`runtime.wp_prepared_select`, `runtime.wp_prepared_update`,
`runtime.wp_prepared_insert`, and `runtime.wp_prepared_delete` prepare one
WordPress-shaped statement before warmup, then reset, bind, and execute that
same native statement for every measured operation. These scenarios isolate
retained-plan execution from prepare cost and accept `--profile-json`; measured
normalization and parse counts must remain zero.

The following focused stress scenarios are listed by `--list` but run only when
selected explicitly with `--scenario`:

- `runtime.cold_open`
- `runtime.large_in_256` and `runtime.large_in_4096`
- `runtime.large_or_2048`
- `runtime.scalar_projection_128`
- `runtime.grouped_projection_128`
- `runtime.wide_order_128`
- `runtime.wide_projection_16` and `runtime.wide_projection_128`
- `runtime.catalog_cache_saturation`
- `runtime.metadata_columns_128`
- `runtime.catalog_ddl_generations`
- `runtime.concurrent_read_write`
- `runtime.processlist_concurrent_8`

`runtime.cold_open` creates and seals one file before timing repeated
`mylite_open()`/`mylite_close()` pairs. `runtime.reopen_query` additionally
executes one indexed point read after every reopen. They separate clean reopen
cost from reopen-plus-query cost and are included in paired performance
regression runs.

`runtime.catalog_ddl_generations` repeatedly changes a column comment and then
warms table metadata. It reports `peak_retained_bytes` and fails if completed
catalog generations or their cold strings escape the catalog cache byte budget.
`runtime.metadata_columns_128` projects and orders 1,024 COLUMNS rows from 128
eight-column tables. `runtime.processlist_concurrent_8` measures PROCESSLIST
snapshot throughput while eight independent handles publish session state.

Invalid-SQL recovery has a separate scaling benchmark:

```sh
build/ci/packages/libmylite/mylite_parser_recovery_benchmark 500 192
```

It reports nested expression and table-reference recovery cost, tokenization
passes, and retry callback counts. The paired regression manifest selects the
expression shape at depth 192 and rejects timing, retry-count, or output-schema
regressions.

Runtime databases are created below `TMPDIR`. Use a tmpfs directory to isolate
CPU and allocator cost, or point `TMPDIR` at a block-backed filesystem to retain
journal and synchronization latency in the measurement. Record the filesystem
type with benchmark results; the two modes are not directly comparable.

## Source Coverage

The Clang coverage preset runs the complete native suite and records one raw
profile per test process:

```sh
CC=clang cmake --preset coverage
cmake --build --preset coverage
mkdir -p build/coverage/profiles
ctest --preset coverage --output-on-failure
tools/generate-coverage-report
```

The report is written below `build/coverage/coverage-report`. Its text and JSON
summaries aggregate line, function, and branch coverage for the runtime, SQL
frontend, and storage modules. `tools/coverage-thresholds.tsv` contains the
checked module ratchets; a missing module or a regression below any threshold
fails report generation. Coverage uses a nonshipping shared build so white-box
tests exercise the same instrumented MyLite and bundled SQLite objects.

## Runtime Phase Profiling

An opt-in Release preset adds phase counters without affecting production
builds:

```sh
cmake --preset perf-profile
cmake --build --preset perf-profile --target mylite_benchmark
build/perf-profile/packages/libmylite/mylite_benchmark \
  --only runtime \
  --scenario runtime.wp_frontend_request \
  --iterations 1000 \
  --samples 7 \
  --warmup 2 \
  --profile-json build/perf-profile/wp-frontend.jsonl
```

`--profile-json` truncates the destination once and appends one JSON object per
measured sample. Each object includes wall time and these cumulative counters:

- `statement_api_ns`: time inside buffered statement execution or cursor
  preparation.
- `normalization_ns` and `parse_ns`: compatibility normalization and MyLite
  lex/parse time.
- `normalization_count` and `parse_count`: phase invocation counts used to
  detect reparsing independently of timer noise.
- `parser_retry_callback_count` and `parser_retry_handled_count`: parser
  recovery callbacks attempted and statements accepted by a retry path.
- `sqlite_step_ns`: time spent inside runtime `sqlite3_step()` calls, measured
  with MyLite's monotonic clock.
- `metadata_step_ns` and `metadata_step_count`: the subset of SQLite stepping
  performed by MyLite catalog and physical-schema metadata access.
- `allocation_count` and `allocation_bytes`: successful libc allocation calls
  and requested bytes from the instrumented MyLite target. Reallocations count
  their newly requested size; frees do not subtract from the cumulative total.
- `descriptor_copy_count` and `descriptor_copy_bytes`: full catalog-column
  descriptor copies performed while loading or cloning planning metadata.
- `execution_statement_cache_*` and `catalog_statement_cache_*`: cache hits,
  misses, evictions, and uncached prepares for generated runtime SQL and
  MyLite catalog SQL, respectively.
- `result_buffer_ns`: time copying rows into buffered MyLite results.
- `cursor_step_ns`: time spent stepping cursor results.
- `cursor_finalize_ns`: time finalizing cursor statements and their transaction
  state.
- `unattributed_ns`: saturating remainder after subtracting normalization,
  parse, SQLite step, and result-buffer time from the profiled API time.
- Statement, SQLite step, cursor-finalize, buffered/cursor row, and result-value
  byte counts used to normalize the timings.

The unattributed remainder includes planning, compatibility evaluation,
metadata, catalog, transaction, allocation, and other uninstrumented MyLite
work outside `sqlite3_step()`. SQLite scalar callbacks execute inside a step and
are therefore included in `sqlite_step_ns`. The profiler is an internal
benchmark facility rather than a public libmylite ABI.
Use this preset to locate phase and call-count gaps, then confirm absolute
performance with the normal `ci` Release build because instrumentation adds
clock overhead.

`runtime.group_concat_distinct_1024` measures the collation-aware DISTINCT
aggregate path with 1,024 unique string values. It is a focused scaling
scenario for duplicate-key construction and hash-table growth; use the normal
Release build for comparisons because allocator and instrumentation choices
materially affect this workload.

## Regression Qualification

`tools/compare-performance` compares Release benchmark binaries from a baseline
and candidate revision using the scenarios and tolerances in
`tools/performance-scenarios.tsv`. Each scenario runs in ABBA order, producing
14 baseline and 14 candidate samples. The gate compares medians and adds a
three-standard-error allowance derived from scaled median absolute deviation.
It rejects excessively noisy runs instead of silently widening the tolerance.
The manifest covers the main benchmark plus the separate parser-recovery
binary, and includes WordPress requests, retained prepared execution, large
`IN` planning, cache saturation, high-cardinality metadata, and concurrent
PROCESSLIST publication.

The scheduled and pull-request performance workflow builds both revisions on
one runner, pins the comparison to one allowed CPU, and uses tmpfs for runtime
databases. Raw CSV, robust statistics, revision IDs, compiler version, kernel,
and CPU model are retained for 90 days. Only paired measurements are treated as
regression evidence; absolute values from different hosted runners are history,
not directly comparable baselines.

The weekly and manually dispatched workflow also runs all 57 native
large-dataset scenarios at 100,000 rows, followed by the reopen, concurrency,
and storage-lifecycle system suite. Manual runs accept 1,000 through 1,000,000
rows. The job retains raw CSV and paired MyLite/SQLite summaries for 90 days.
Correctness mismatches and worker errors fail the job, while timings remain
evidence rather than absolute hosted-runner gates.

## WordPress MySQL Server-Test Query CSV

The WordPress SQLite integration project publishes a MySQL server-test query
CSV. It is not checked into this repository because it is an external generated
dataset and is several megabytes.

The file uses one SQL statement per nonempty row. Rows are usually double-quoted
and may contain physical newlines. Inside quoted rows, the benchmark loader
accepts both doubled `""` quotes and MySQL-style backslash-escaped `\"` quotes,
matching the generated corpus format.

Fetch it into the build tree:

```sh
mkdir -p build/perf-data
curl -L --fail \
  -o build/perf-data/mysql-server-tests-queries.csv \
  https://raw.githubusercontent.com/WordPress/sqlite-database-integration/trunk/packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv
```

Run the CSV lexer and parser benchmarks:

```sh
build/ci/packages/libmylite/mylite_benchmark \
  --csv build/perf-data/mysql-server-tests-queries.csv \
  --csv-iterations 1 \
  --only lexer

build/ci/packages/libmylite/mylite_benchmark \
  --csv build/perf-data/mysql-server-tests-queries.csv \
  --csv-iterations 1 \
  --only parse
```

The CSV contains MySQL server-test statements, so parser benchmarks report
syntax/lexer status counts instead of treating unsupported syntax as a benchmark
failure.

The CSV is a flattened statement corpus, not a single guaranteed MySQL session.
By default, each row is parsed with the default lexer mode mask. To diagnose
mode-sensitive parser gaps, add `--csv-replay-sql-mode`; this tracks recognized
session `SET sql_mode` changes across rows, but its score is not directly
comparable to the default standalone-row corpus score.

To inspect the remaining parser misses, add `--dump-parse-failures PATH` to a
parse CSV benchmark:

```sh
build/ci/packages/libmylite/mylite_benchmark \
  --csv build/perf-data/mysql-server-tests-queries.csv \
  --csv-iterations 1 \
  --only parse \
  --dump-parse-failures build/perf-data/mysql-server-tests-parse-failures.tsv
```

The dump is tab-separated and includes the one-based CSV row number, parse
status, error-token kind and position, escaped error-token text, and escaped SQL
text. It is intended for local triage and is not required by CI.
