# Baseline ALTER TABLE DROP INDEX Lifecycle Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  `ALTER TABLE ... DROP INDEX` / `DROP KEY`, metadata, diagnostics, affected
  rows, auto-increment key restrictions, and deferred wider syntax.
- [x] Add MySQL-runtime expectation script covering successful metadata,
  diagnostics, auto-increment interaction, unique-index removal, and deferred
  upstream-accepted forms.
- [x] Add parser/AST support for the admitted single-action
  `ALTER TABLE table_name DROP INDEX|KEY index_name` subset and parser tests
  for supported and unsupported forms.
- [x] Add analyzer/planner support for schema/table resolution, base-table
  validation, descriptor index resolution, secondary-only validation,
  auto-increment key preservation checks, and cleanup-safe zero initialization.
- [x] Add catalog/runtime execution to delete index/index-column descriptors
  atomically, drop the generated SQLite physical index from descriptors, update
  table descriptor generations, and preserve rows and column descriptors.
- [x] Ensure `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, `CREATE TABLE ... LIKE`, DML, reopen, rename/drop, and
  independent handles observe the post-drop descriptor state.
- [x] Add fast C runtime coverage for success cases, metadata, persistence,
  file-format safety, auto-increment interaction, diagnostics, and unsupported
  syntax.
- [x] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`, and
  `docs/compatibility/sql-table-ddl.md` with exact limited wording.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, catalog authority, physical SQL
  quoting, cleanup on failure, performance, scope control, compatibility docs,
  and test relevance.
