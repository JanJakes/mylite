# Parser Corpus SELECT Clause Residuals Tasks

- [x] Verify representative MySQL 8.4.9 syntax and runtime expectations.
- [x] Specify SELECT clause residual boundaries.
- [x] Add MySQL expectation script.
- [x] Add parser tests for tableless limits, repeated locking clauses, and
  residual placeholders.
- [x] Add runtime tests for executable limits, locking no-ops, and unsupported
  diagnostics.
- [x] Implement parser and runtime support.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark before implementation:

- `ok=69288`, `syntax_error=285`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.

Parser corpus benchmark after implementation:

- `ok=69315`, `syntax_error=258`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
