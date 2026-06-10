# Parser Corpus Query Function And Subquery Surfaces Tasks

- [x] Verify representative MySQL 8.4.9 syntax and runtime expectations.
- [x] Specify query function/subquery fallback boundaries.
- [x] Add parser tests for function/subquery placeholder admission.
- [x] Add runtime tests for unsupported function/subquery diagnostics.
- [x] Implement parser placeholder support without changing execution
  semantics.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark before implementation:

- `ok=67318`, `syntax_error=2255`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.

Parser corpus benchmark after implementation:

- `ok=68405`, `syntax_error=1168`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
