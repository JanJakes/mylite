# Baseline DATEDIFF Function Tasks

- [x] Review existing temporal scalar, row-scalar, parser, SQLite callback, and
  compatibility documentation patterns.
- [x] Verify `DATEDIFF()` behavior against MySQL 8.4.9, including invalid
  inputs, zero dates, year-zero complete dates, `NULL`, wrong arities,
  whitespace, and identifier use.
- [x] Write the independently authored feature spec.
- [x] Add MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Add parser/AST support for `DATEDIFF(expr1, expr2)` and wrong arities.
- [x] Add the MyLite-owned `DATEDIFF()` runtime helper and SQLite scalar
  callback.
- [x] Wire scalar and row-scalar analyzer/planner/execution paths.
- [x] Add focused C runtime tests and parser coverage.
- [x] Update compatibility documentation.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, amend issues, commit atomically, and push `main`.
