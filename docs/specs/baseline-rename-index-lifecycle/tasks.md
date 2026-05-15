# Baseline RENAME INDEX Lifecycle Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  `ALTER TABLE ... RENAME INDEX|KEY`, metadata, diagnostics, affected rows,
  same-name/case-only renames, primary-name rejection, foreign-key interaction,
  and deferred wider syntax.
- [x] Add MySQL-runtime expectation script covering successful metadata,
  diagnostics, unique-index enforcement after rename, foreign-key-backed
  indexes, and deferred upstream-accepted forms.
- [ ] Add parser/AST support for the admitted single-action
  `ALTER TABLE table_name RENAME INDEX|KEY old TO new` subset and parser tests
  for supported and unsupported forms.
- [ ] Add analyzer/planner support for schema/table resolution, base-table
  validation, descriptor index resolution, primary-name rejection, duplicate
  new-name checks, same-name/case-only handling, and cleanup-safe zero
  initialization.
- [ ] Add catalog/runtime execution to rename the logical index descriptor
  atomically, update table/index descriptor generations, preserve the physical
  SQLite index, and keep rows and index-column descriptors unchanged.
- [ ] Ensure `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, `CREATE TABLE ... LIKE`, DML, reopen, rename/drop,
  foreign-key enforcement, and independent handles observe the renamed
  descriptor state.
- [ ] Add fast C runtime coverage for success cases, metadata, persistence,
  file-format safety, foreign-key interaction, diagnostics, and unsupported
  syntax.
- [ ] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`, and
  `docs/compatibility/sql-table-ddl.md` with exact limited wording.
- [ ] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, catalog authority, physical SQL
  avoidance, cleanup on failure, performance, scope control, compatibility
  docs, and test relevance.
