# Baseline SHOW COLUMNS WHERE Tasks

- [x] Read project architecture, existing `SHOW COLUMNS`, `SHOW FULL COLUMNS`,
  `SHOW VARIABLES WHERE`, `SHOW TABLE STATUS WHERE`, `SHOW INDEX WHERE`, parser,
  runtime, result, catalog, storage, and compatibility context.
- [x] Verify MySQL 8.4.9 behavior for `SHOW COLUMNS ... WHERE` and
  `SHOW FULL COLUMNS ... WHERE` syntax, output columns, predicate shapes,
  diagnostics, warning count, and `ROW_COUNT()`.
- [x] Specify the independently authored MyLite syntax and runtime subset.
- [x] Extend parser/AST support for trailing `WHERE` filters on `SHOW COLUMNS`,
  `SHOW FIELDS`, `SHOW FULL COLUMNS`, and `SHOW FULL FIELDS`.
- [x] Add MySQL-runtime-verified expectation coverage for admitted and deferred
  behavior.
- [x] Implement descriptor-row predicate validation and filtering in the
  existing `SHOW COLUMNS` runtime path.
- [x] Extend parser and runtime C tests for successful filters, diagnostics,
  metadata, persistence, and file-format safety.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run targeted parser/runtime tests and the new MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, run a review subagent, amend if
  needed, and push `main`.
