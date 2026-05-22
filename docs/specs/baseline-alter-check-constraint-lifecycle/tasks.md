# Baseline ALTER CHECK Constraint Lifecycle Tasks

- [x] Read current CHECK, parser, catalog, table rebuild, metadata, DML,
  storage, and SQLite integration context.
- [x] Verify MySQL 8.4.9 ALTER CHECK behavior for syntax, names, existing-row
  validation, affected rows, diagnostics, metadata, and enforcement toggles.
- [x] Write the independent feature spec with ownership boundaries, grammar
  snippets, physical rebuild design, diagnostics, and tests.
- [x] Add a MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Extend parser/AST support for `ALTER TABLE ... ADD CHECK`,
  `DROP CHECK`, and `ALTER CHECK`.
- [x] Add catalog APIs for deleting one CHECK descriptor and updating
  enforcement metadata in a mutation.
- [x] Implement descriptor planning for add/drop/toggle, generated names,
  duplicate-name checks, expression validation, and existing-check lookup.
- [x] Implement physical table rebuild with descriptor-preserved columns,
  enforced checks, indexes, rows, and file-format safety.
- [x] Preserve metadata rendering through `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`, and `TABLE_CONSTRAINTS`.
- [x] Add focused parser/runtime tests and register any new test binary.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority, physical
  SQLite scope, diagnostics, metadata, performance, persistence, cleanup, and
  scope control.
- [x] Commit, review with a subagent, amend if needed, and push `main`.
