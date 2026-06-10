# Parser Corpus Parenthesized Table Reference Surfaces Tasks

- [x] Verify representative MySQL 8.4.9 syntax and runtime expectations.
- [x] Specify parenthesized table-reference grammar and runtime boundaries.
- [x] Add parser tests for parenthesized base, joined, and comma table
  references.
- [x] Add runtime tests for existing flat joins and unsupported parenthesized
  table-reference diagnostics.
- [x] Implement parser placeholder support without flattening
  precedence-sensitive nested joins.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark after implementation:

- `ok=66831`, `syntax_error=2742`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
