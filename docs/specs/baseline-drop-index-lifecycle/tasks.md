# Baseline DROP INDEX Lifecycle Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  standalone `DROP INDEX`, metadata, diagnostics, affected rows, warnings,
  auto-increment key restrictions, and deferred wider syntax.
- [x] Add MySQL-runtime expectation script covering successful metadata,
  diagnostics, auto-increment interaction, unique-index removal, schema
  resolution, and deferred upstream-accepted forms.
- [x] Add parser/AST support for the admitted
  `DROP INDEX index_name ON table_name` subset and parser tests for supported
  and unsupported forms.
- [x] Refactor/reuse drop-index planning so standalone and `ALTER TABLE`
  forms resolve schema/table/index descriptors consistently.
- [x] Add catalog/runtime execution for standalone drops to delete
  index/index-column descriptors atomically, drop the generated SQLite physical
  index from descriptors, update table descriptor generations, and preserve
  rows and column descriptors.
- [x] Ensure `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, `CREATE TABLE ... LIKE`, DML, reopen, rename/drop, and
  independent handles observe the post-drop descriptor state.
- [x] Add fast C runtime coverage for success cases, metadata, persistence,
  file-format safety, auto-increment interaction, diagnostics, and unsupported
  syntax.
- [x] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`,
  `docs/compatibility/sql-table-ddl.md`, and related feature specs with exact
  limited wording.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, catalog authority, physical SQL
  quoting, cleanup on failure, performance, scope control, compatibility docs,
  and test relevance.
