# Baseline CHECK Constraint Lifecycle Tasks

- [x] Read current architecture, parser, catalog, table lifecycle, key,
  foreign-key, DML, information-schema, storage, and SQLite integration
  context.
- [x] Verify MySQL 8.4.9 check-constraint behavior for names, enforcement,
  expressions, DML diagnostics, metadata, `LIKE`, CTAS, rename, and drop.
- [x] Write the independent feature spec with ownership boundaries, grammar
  snippets, catalog design, physical SQLite handling, diagnostics, and tests.
- [x] Add a MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Extend parser/AST support for table-level and inline column checks.
- [x] Add durable MyLite check-constraint catalog descriptors and migration.
- [x] Implement descriptor planning, expression validation, generated names,
  duplicate-name checks, and SQLite expression rendering.
- [x] Add physical enforced `CHECK` clauses while preserving MyLite descriptor
  authority.
- [x] Add metadata rendering through `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`, and `TABLE_CONSTRAINTS`.
- [x] Add DML violation mapping and `INSERT IGNORE` warning/skipped-row
  behavior for supported row-write paths.
- [x] Add focused parser/runtime tests and register any new test binary.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority,
  physical SQLite scope, diagnostics, metadata, performance, persistence,
  file-format safety, cleanup, and scope control.
- [x] Commit, review with a subagent, amend if needed, and push `main`.
