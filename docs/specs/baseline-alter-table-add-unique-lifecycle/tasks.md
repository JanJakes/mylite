# Baseline ALTER TABLE ADD UNIQUE Lifecycle Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  `ALTER TABLE ... ADD UNIQUE`, metadata, duplicate validation, diagnostics,
  affected rows, warnings, and deferred wider syntax.
- [x] Add a MySQL-runtime expectation script covering successful metadata,
  diagnostics, duplicate validation, unsupported-but-accepted upstream forms,
  and schema resolution.
- [x] Add parser/AST support for the admitted
  `ALTER TABLE table_name ADD UNIQUE [INDEX|KEY] [index_name] (column_name)`
  subset and parser tests for supported and unsupported forms.
- [x] Add analyzer/planner support for schema/table resolution, base-table
  validation, descriptor column resolution, duplicate index-name checks,
  supported target type checks, unique duplicate prevalidation, and cleanup-safe
  zero initialization.
- [x] Add catalog/runtime execution that reuses descriptor-owned secondary
  index creation with `is_unique = true`, creates the generated SQLite unique
  index from descriptors, updates descriptor generations, and preserves rows
  and column descriptors.
- [x] Ensure `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, `CREATE TABLE ... LIKE`, DML, `ALTER TABLE ... DROP
  INDEX|KEY`, standalone `DROP INDEX`, reopen, and independent handles observe
  the new descriptor through existing paths.
- [x] Add fast C runtime coverage for success cases, metadata, persistence,
  unique duplicates, file-format safety, diagnostics, unsupported syntax, and
  later DML enforcement.
- [x] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`,
  `docs/compatibility/sql-table-ddl.md`, and related feature specs with exact
  limited wording.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, catalog authority, physical SQL
  quoting, cleanup on failure, performance, scope control, compatibility docs,
  and test relevance.
- [x] Extend the slice for `ALTER TABLE ... ADD CONSTRAINT [symbol] UNIQUE`
  forms by verifying MySQL 8.4.9 metadata behavior, reusing the existing
  unique-index descriptor path, updating parser/runtime coverage, and keeping
  compatibility docs limited to visible unique-index metadata rather than a
  separate unique-constraint descriptor.
