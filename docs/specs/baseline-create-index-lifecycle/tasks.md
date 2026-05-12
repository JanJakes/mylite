# Baseline CREATE INDEX Lifecycle Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  standalone `CREATE INDEX` / `CREATE UNIQUE INDEX`, metadata, diagnostics,
  duplicate validation, row counts, warnings, and deferred wider syntax.
- [x] Add MySQL-runtime expectation script covering successful metadata,
  diagnostics, unique duplicate validation, unsupported-but-accepted upstream
  forms, and schema resolution.
- [x] Add parser/AST support for the admitted explicit-name
  `CREATE [UNIQUE] INDEX index_name ON table_name (column_name)` subset and
  parser tests for supported and unsupported forms.
- [x] Add analyzer/planner support for schema/table resolution, base-table
  validation, descriptor column resolution, duplicate index-name checks,
  supported target type checks, unique duplicate prevalidation, and cleanup-safe
  zero initialization.
- [x] Add catalog/runtime execution to allocate an index descriptor, insert
  index/index-column rows atomically, create the generated SQLite physical
  index from descriptors, update descriptor generations, and preserve table
  rows and column descriptors.
- [x] Ensure `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, `CREATE TABLE ... LIKE`, DML,
  `ALTER TABLE ... DROP INDEX|KEY`, reopen, and independent handles observe
  the new descriptor through existing paths.
- [x] Add fast C runtime coverage for success cases, metadata, persistence,
  unique duplicates, file-format safety, diagnostics, and unsupported syntax.
- [x] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`, and
  `docs/compatibility/sql-table-ddl.md` with exact limited wording.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, catalog authority, physical SQL
  quoting, cleanup on failure, performance, scope control, compatibility docs,
  and test relevance.
