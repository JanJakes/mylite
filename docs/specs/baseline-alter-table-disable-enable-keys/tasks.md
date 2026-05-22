# Baseline ALTER TABLE DISABLE/ENABLE KEYS Tasks

Add a narrow no-op compatibility slice for MyISAM-style key-maintenance ALTER
syntax over current MyLite base tables.

## Checklist

- [x] Verify MySQL 8.4.9 behavior for supported forms, diagnostics, result
  shape, warning/note count, row count, and row/metadata preservation.
- [x] Specify syntax, runtime semantics, diagnostics, architecture boundaries,
  SQLite no-op behavior, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Add parser grammar, AST kinds/names, and parser helpers for
  `ALTER TABLE ... DISABLE KEYS` / `ENABLE KEYS`.
- [x] Add runtime planning and execution that resolves target descriptors,
  validates ALTER options, appends the storage-engine note, and preserves
  catalog/SQLite state.
- [x] Add C parser/runtime tests for success paths, diagnostics, result shape,
  note details, metadata preservation, preamble preservation, reopen behavior,
  and independent handles.
- [x] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, no-op semantics,
  diagnostics, docs accuracy, cleanup, performance, and test relevance.
