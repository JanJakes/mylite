# Baseline DROP FOREIGN KEY Lifecycle Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  `ALTER TABLE ... DROP FOREIGN KEY`, metadata, diagnostics, affected rows,
  child-index preservation, and deferred wider syntax.
- [x] Add a MySQL-runtime expectation script covering successful drops,
  metadata, diagnostics, generated FK names, case-insensitive FK names, and
  deferred upstream-accepted forms.
- [x] Add parser/AST support for the admitted single-action
  `ALTER TABLE table_name DROP FOREIGN KEY foreign_key_name` subset and parser
  tests for supported and unsupported forms.
- [x] Add analyzer/planner support for schema/table resolution, base-table
  validation, descriptor FK resolution, zero-initialized cleanup, and
  deterministic diagnostics.
- [x] Add catalog/runtime execution to delete FK-column and FK descriptors
  atomically, preserve child indexes and row data, and update descriptor
  generations without SQLite physical DDL.
- [x] Ensure `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`,
  `REFERENTIAL_CONSTRAINTS`, DML enforcement, reopen, rename/drop, and
  independent handles observe the post-drop descriptor state.
- [x] Add fast C runtime coverage for success cases, metadata, persistence,
  file-format safety, diagnostics, and unsupported syntax.
- [x] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`, and
  `docs/compatibility/sql-table-ddl.md` with exact limited wording.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff with a subagent, amend any findings, commit, and
  push to remote `main`.
