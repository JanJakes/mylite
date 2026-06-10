# Parser Corpus Function Expression Placeholders Tasks

- [x] Review remaining parser corpus failure buckets and select a coherent
  high-volume function-expression placeholder slice.
- [x] Verify representative MySQL 8.4.9 syntax.
- [x] Add parser tests for function-expression placeholder classification and
  malformed syntax preservation.
- [x] Add runtime tests for unsupported-utility diagnostics.
- [x] Add MySQL expectation script for representative accepted forms.
- [x] Implement placeholder classification.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark after implementation:

- `ok=68593`, `syntax_error=980`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
