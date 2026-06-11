# Parser Corpus Benchmark CSV Loader

This slice fixes the local benchmark harness that reads the WordPress
SQLite-integration MySQL server-test query corpus. It does not change MyLite SQL
syntax, runtime behavior, or compatibility status. The goal is to make parser
coverage measurements reflect MyLite parser behavior rather than row-decoding
artifacts from the external corpus file.

## Source Corpus

The benchmark input is the generated query file documented in
`docs/performance/benchmarks.md`:

```text
https://raw.githubusercontent.com/WordPress/sqlite-database-integration/trunk/packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv
```

Each nonempty row represents one SQL statement. Rows are usually wrapped in
double quotes, and statement text may span physical newlines.

The corpus uses two quote encodings inside quoted rows:

- RFC-style doubled quotes for SQL text that contains `"` bytes, such as
  ANSI_QUOTES identifier examples;
- MySQL backslash-escaped quotes inside SQL string literals, such as
  `year=\"`.

The old benchmark loader recognized doubled quotes but treated a backslash
before `"` as ordinary data followed by a closing row quote. That split valid
multiline SQL rows into fragments and made the parse-failure dump include
loader artifacts such as standalone `FROM`, `HAVING`, `ELSE`, and `END IF`
rows.

## Behavior

`mylite_benchmark` must preserve the existing benchmark CLI and output format.
For the corpus loader:

- blank physical rows are ignored;
- unquoted rows are still accepted as one statement up to end-of-line;
- quoted rows may contain physical newlines;
- `""` inside a quoted row decodes to one `"` byte;
- `\"` inside a quoted row decodes to two bytes, backslash plus quote;
- nonempty decoded rows are appended to the benchmark query list in source
  order.

This is a corpus-loader rule, not a SQL lexer rule. The SQL parser still decides
whether the decoded statement is valid MySQL-compatible syntax.

## Architecture

The CSV loader lives in a small benchmark-local module:

- `packages/libmylite/benchmarks/mylite_benchmark_csv.h`
- `packages/libmylite/benchmarks/mylite_benchmark_csv.c`

`mylite_benchmark.c` uses that module for `--csv` runs and keeps benchmark
measurement logic separate from corpus decoding. The loader is covered by
`packages/libmylite/tests/benchmark_csv_loader_test.c` through a normal CTest
target.

## Tests

The loader test covers:

- doubled-quote rows such as `select ""$id2""`;
- backslash-quote rows such as `year=\"` and embedded `\"\r` fragments inside
  multiline SQL;
- a mix of plain, blank, and quoted rows.

After the loader change, the parser-corpus benchmark must be rerun with
`--dump-parse-failures` so the remaining failure groups are based on decoded
statements rather than split CSV fragments.
