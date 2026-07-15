# MyLite Benchmarks

MyLite performance benchmarks are explicit developer tools, not default tests.
Build them with a Release configuration when comparing timings:

```sh
cmake --preset ci
cmake --build --preset ci --target mylite_benchmark
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
operation times. Pinning a run to an otherwise idle CPU core, where supported,
reduces scheduler noise. `--per-query` expands each selected request scenario
into independently measured `.queryN` scenarios.

The benchmark reports CSV rows:

```text
scenario,kind,iterations,queries,operations,ok,errors,tokens,bytes,total_ms,avg_us,ops_per_sec
```

`kind=parse` measures lexing plus parsing into a MyLite AST and then freeing
the parse result. `kind=lexer` measures tokenization only. `kind=execute`
measures `mylite_execute()` against a temporary file-backed MyLite database and
does not use PHP, WordPress, or the PHP extensions.

`runtime.wp_frontend_request` executes six representative WordPress frontend
reads per iteration. `runtime.wp_write_request` executes a five-query mixed
request containing a read, an upsert, two updates, and a delete. These scenarios
measure a request-shaped sequence while retaining the existing `.queryN`
isolation for hotspot analysis.

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
- `sqlite_step_ns`: time spent inside runtime `sqlite3_step()` calls, measured
  with MyLite's monotonic clock.
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
