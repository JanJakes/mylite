# Parser Corpus CTE Placeholder Surfaces Tasks

- [x] Classify remaining parser corpus CTE failures and choose a parser-only
      placeholder slice.
- [x] Specify CTE placeholder boundaries and deferred execution semantics.
- [x] Add MySQL 8.4.9 expectation probes for representative CTE forms.
- [x] Add parser tests for CTE placeholder admission.
- [x] Add runtime tests for unsupported CTE diagnostics.
- [x] Implement parser placeholder classification without adding broad Lemon
      grammar.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark before implementation:

- `ok=69008`, `syntax_error=565`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.

Parser corpus benchmark after implementation:

- `ok=69037`, `syntax_error=536`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
