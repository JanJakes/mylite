# Baseline TIMESTAMPDIFF Function Tasks

- [x] Review existing temporal scalar, row-scalar, parser, SQLite callback, and
  compatibility documentation patterns.
- [x] Verify `TIMESTAMPDIFF()` behavior against MySQL 8.4.9, including units,
  SQL_TSI aliases, invalid inputs, zero dates, year-zero complete dates,
  `NULL`, parse errors, whitespace, and identifier use.
- [x] Write the independently authored feature spec.
- [x] Add MySQL 8.4.9 expectation script for the user-visible behavior.
- [ ] Add parser/AST support for `TIMESTAMPDIFF(unit, expr1, expr2)`.
- [ ] Add the MyLite-owned `TIMESTAMPDIFF()` runtime helper and SQLite scalar
  callback.
- [ ] Wire scalar and row-scalar analyzer/planner/execution paths.
- [ ] Add focused C runtime tests and parser coverage.
- [ ] Update compatibility documentation.
- [ ] Run focused build/tests and MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, amend issues, commit atomically, and push `main`.
