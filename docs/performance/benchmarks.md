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

The benchmark reports CSV rows:

```text
scenario,kind,iterations,queries,operations,ok,errors,tokens,bytes,total_ms,avg_us,ops_per_sec
```

`kind=parse` measures lexing plus parsing into a MyLite AST and then freeing
the parse result. `kind=lexer` measures tokenization only. `kind=execute`
measures `mylite_execute()` against a temporary file-backed MyLite database and
does not use PHP, WordPress, or the PHP extensions.

## WordPress MySQL Server-Test Query CSV

The WordPress SQLite integration project publishes a MySQL server-test query
CSV. It is not checked into this repository because it is an external generated
dataset and is several megabytes.

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
