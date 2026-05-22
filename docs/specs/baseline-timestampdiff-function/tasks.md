# Baseline TIMESTAMPDIFF Function Tasks

- [x] Review existing temporal scalar, row-scalar, parser, SQLite callback, and
  compatibility documentation patterns.
- [x] Verify `TIMESTAMPDIFF()` behavior against MySQL 8.4.9, including units,
  SQL_TSI aliases, invalid inputs, zero dates, year-zero complete dates,
  `NULL`, parse errors, whitespace, and identifier use.
- [x] Write the independently authored feature spec.
- [x] Add MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Add parser/AST support for `TIMESTAMPDIFF(unit, expr1, expr2)`.
- [x] Add the MyLite-owned `TIMESTAMPDIFF()` runtime helper and SQLite scalar
  callback.
- [x] Wire scalar and row-scalar analyzer/planner/execution paths.
- [x] Add focused C runtime tests and parser coverage.
- [x] Update compatibility documentation.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, amend issues, commit atomically, and push `main`.
