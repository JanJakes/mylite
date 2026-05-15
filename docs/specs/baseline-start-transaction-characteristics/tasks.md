# Baseline START TRANSACTION Characteristics Tasks

- [x] Research official MySQL 8.4 `START TRANSACTION` characteristic syntax
  and scope rules.
- [x] Verify MySQL 8.4.9 runtime behavior for success forms, duplicate and
  contradictory characteristics, `WITH CONSISTENT SNAPSHOT` warnings, read-only
  persistent writes, temporary-table DML, nested start, DDL implicit commit, and
  pending/session `SET TRANSACTION` interactions.
- [x] Specify the MyLite grammar, ownership boundaries, runtime semantics,
  diagnostics, non-goals, SQLite/storage handling, performance shape, and test
  plan.
- [x] Add MySQL expectation script for this feature.
- [x] Add parser and AST support for the supported grammar subset.
- [x] Add runtime handling for statement-level transaction characteristics.
- [x] Preserve existing `SET TRANSACTION`, savepoint, lock-table, temporary
  table, DDL implicit-commit, and read-only DML behavior.
- [x] Add runtime C tests and parser tests.
- [x] Update `COMPATIBILITY.md` and SQL transaction compatibility docs.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `main`.
