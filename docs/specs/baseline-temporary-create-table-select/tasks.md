# Baseline Temporary CREATE TABLE SELECT Tasks

- [x] Read project architecture, existing persistent CTAS, temporary table, and
  temporary LIKE specs.
- [x] Research official MySQL 8.4 `CREATE TEMPORARY TABLE` and
  `CREATE TABLE ... SELECT` documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for supported temporary CTAS forms,
  result reporting, metadata visibility, name resolution, shadowing, and
  `IF NOT EXISTS`.
- [x] Specify the limited grammar, ownership boundaries, resolution rules,
  descriptor inference, generated SQLite handling, diagnostics, performance,
  and test plan.
- [x] Add MySQL-runtime expectation script for the feature.
- [x] Extend parser/AST support for `CREATE TEMPORARY TABLE ... [AS] SELECT`.
- [x] Implement temporary CTAS execution using descriptor inference,
  SQLite-side row copy, temporary descriptor append-after-copy, and cleanup on
  failure.
- [x] Add focused parser and runtime C tests.
- [x] Update compatibility documentation.
- [x] Run focused verification and `cmake --workflow --preset check`.
- [x] Review, amend if needed, commit, and push.
