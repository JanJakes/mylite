# Parser Corpus DML Variant Surfaces Tasks

- [x] Verify representative MySQL 8.4.9 DML variant syntax and results.
- [x] Specify executable no-op delete modifiers and placeholder boundaries.
- [x] Add parser tests for no-op delete modifiers and DML variant placeholders.
- [x] Add runtime tests for delete modifier execution and unsupported
  placeholder diagnostics.
- [x] Implement grammar and parser fallback classification changes.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark before implementation:

- `ok=69159`, `syntax_error=414`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.

Parser corpus benchmark after implementation:

- `ok=69224`, `syntax_error=349`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
- `parse.csv.mysql_server_tests`: `2632.165 ms` total, `37.821 us/query`,
  `26440.216 queries/sec`.
