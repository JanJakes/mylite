# Baseline SHOW INDEX WHERE Tasks

- [x] Read project architecture, `SHOW INDEX`, `SHOW VARIABLES WHERE`,
  `SHOW TABLE STATUS WHERE`, index metadata, parser, runtime, and compatibility
  context.
- [x] Verify MySQL 8.4.9 behavior for `SHOW INDEX ... WHERE` syntax, output
  columns, supported predicate shapes, diagnostics, warning count, and
  `ROW_COUNT()`.
- [x] Specify the independently authored MyLite syntax and runtime subset.
- [x] Extend parser/AST support for a trailing `WHERE` filter on `SHOW INDEX`,
  `SHOW INDEXES`, and `SHOW KEYS`.
- [x] Add MySQL-runtime-verified expectation coverage for the admitted and
  deliberately deferred behavior.
- [x] Implement descriptor-row predicate validation and filtering in the
  existing `SHOW INDEX` runtime path.
- [x] Extend parser and runtime C tests for successful filters, diagnostics,
  metadata, persistence, and file-format safety.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run targeted parser/runtime tests and the new MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, run a review subagent, amend if
  needed, and push `main`.
