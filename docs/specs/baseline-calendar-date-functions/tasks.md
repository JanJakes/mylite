# Baseline Calendar Date Functions Tasks

- [x] Review existing temporal-extract, DATEDIFF, parser, SQLite callback, and
  compatibility documentation patterns.
- [x] Verify `DAYOFWEEK()`, `DAYOFYEAR()`, and `LAST_DAY()` behavior against
  MySQL 8.4.9 for valid dates, zero/partial dates, invalid strings, year zero,
  wrong arities, whitespace, identifier use, and SQL modes.
- [x] Write the independently authored feature spec.
- [x] Add MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Add parser/AST support for the three functions and wrong arities.
- [x] Extend the MyLite-owned temporal scalar runtime helper.
- [x] Wire scalar and row-scalar analyzer/planner/execution paths.
- [x] Add focused C runtime tests and parser coverage.
- [x] Update compatibility documentation.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, amend issues, commit atomically, and push `main`.
