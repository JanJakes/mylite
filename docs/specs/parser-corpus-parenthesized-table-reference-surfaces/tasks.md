# Parser Corpus Parenthesized Table Reference Surfaces Tasks

- [x] Verify representative MySQL 8.4.9 syntax and runtime expectations.
- [x] Specify parenthesized table-reference grammar and runtime boundaries.
- [x] Add parser tests for parenthesized base, joined, comma table-reference,
  ODBC join escape, and mixed comma/explicit join surfaces.
- [x] Add runtime tests for existing flat joins and unsupported recognized
  table-reference diagnostics.
- [x] Implement parser placeholder support without flattening
  precedence-sensitive nested joins or mixed comma/explicit join lists.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark after implementation:

- `ok=69116`, `syntax_error=457`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
