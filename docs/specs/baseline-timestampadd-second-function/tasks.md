# Baseline TIMESTAMPADD SECOND Function Tasks

- [x] Review existing temporal scalar, row-scalar, parser, SQLite callback, and
  compatibility documentation patterns.
- [x] Verify `TIMESTAMPADD()` behavior against MySQL 8.4.9 for the intended
  `SECOND`/`SQL_TSI_SECOND` subset and representative deferred behavior.
- [x] Write the independently authored feature spec.
- [x] Add MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Add parser/AST support for `TIMESTAMPADD(unit, interval, value)`.
- [x] Wire scalar and row-scalar analyzer/planner/execution paths through the
  existing MyLite interval-second helper.
- [x] Add focused C runtime tests and parser coverage.
- [x] Update compatibility documentation.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, amend issues, commit atomically, and push `main`.
