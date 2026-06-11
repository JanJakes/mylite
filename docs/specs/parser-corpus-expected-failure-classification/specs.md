# Parser Corpus Expected Failure Classification

This slice adds an opt-in benchmark gate for the residual
`mysql-server-tests-queries.csv` parse failures that should remain failures in
MyLite's default parser mode.

The parser corpus is useful as a broad syntax stress test, but it is not a
single MySQL session and it contains rows that are invalid in MySQL 8.4.9 or
valid only under a different session `sql_mode`. Once MyLite reaches a small
residual set, the raw failure count becomes misleading unless expected
failures are classified separately from unexpected parser gaps.

## Compatibility Authority

The residual classes are grounded in existing MySQL 8.4.9 expectation scripts
and parser-mode behavior:

- removed `SHOW MASTER STATUS`, `SHOW SLAVE STATUS`, and `SHOW SLAVE HOSTS`
  aliases are verified syntax errors in the binary-log and replica metadata
  expectation scripts;
- `LOCK TABLES table LOW_PRIORITY WRITE` is verified as a MySQL 8.4.9 syntax
  error in the lock-tables lifecycle expectation script;
- `FLUSH HOSTS` is a removed MySQL 8.4 surface and is already kept as a syntax
  error by the FLUSH parser-corpus slice;
- double-quoted identifier rows require `ANSI_QUOTES`; the default parser mode
  intentionally treats double quotes as string delimiters;
- incomplete `ALTER TABLE` rows, the malformed numeric literal row, and the
  non-SQL character-set artifact are corpus artifacts rather than valid MySQL
  statements.

## Tooling Semantics

`mylite_benchmark` gains an optional `--expected-parse-failures PATH` argument.
When present with `--csv` and a parse benchmark filter, the benchmark reparses
the CSV rows once after the timed measurement and compares non-OK rows against
the manifest.

The manifest format is tab-separated:

```text
# query_index	status	error_token_kind	reason
508	syntax_error	eof	incomplete ALTER TABLE corpus artifact
```

Indexes are one-based and match `--dump-parse-failures` output. The status and
token fields must match the actual parse result. A matched expected failure is
reported separately; an unexpected parse failure or a manifest entry whose row
now parses successfully fails the benchmark command.

This option is measurement-only. It does not relax MyLite parser behavior and
must not be used to hide production compatibility gaps.

## Runtime Behavior

No runtime SQL behavior changes. The benchmark tool is the only executable
surface changed by this slice.

## Tests

Coverage includes:

- manifest parsing and duplicate/error validation;
- expected, unexpected, and missing parse-failure classification;
- a full current corpus run using the checked-in expected-failure manifest.
