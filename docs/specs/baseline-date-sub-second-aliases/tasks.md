# Baseline DATE_SUB SECOND Aliases Tasks

- [x] Read current architecture, compatibility, parser, scalar-expression,
  temporal-type, and MySQL expectation patterns.
- [x] Verify MySQL 8.4.9 runtime behavior for supported `DATE_SUB`, `ADDDATE`,
  and `SUBDATE` interval-second values, labels, `NULL`, signed intervals,
  date-only promotion, `DO`, SQL-mode whitespace, and identifier behavior.
- [x] Write an independently authored feature spec with MyLite Lemon snippets.
- [x] Add MySQL 8.4.9 expectation artifact.
- [x] Add parser/AST support for the narrow interval-second shapes.
- [x] Implement MyLite-owned scalar evaluator support without SQLite fork
  changes.
- [x] Add fast C parser/runtime tests.
- [x] Update compatibility docs with limited wording.
- [x] Run MySQL expectation script, focused CTests, `cmake --build --preset
  dev`, and `cmake --workflow --preset check`.
- [x] Review final diff for MySQL evidence, scope control, SQL-mode behavior,
  temporal arithmetic correctness, no catalog/storage mutation, cleanup, and
  docs accuracy.
