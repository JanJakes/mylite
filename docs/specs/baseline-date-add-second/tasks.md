# Baseline DATE_ADD SECOND Tasks

- [x] Read current architecture, compatibility, parser, scalar-expression,
  temporal-type, and MySQL expectation patterns.
- [x] Verify MySQL 8.4.9 runtime behavior for supported `DATE_ADD(... INTERVAL
  ... SECOND)` values, labels, `NULL`, signed intervals, date-only promotion,
  `DO`, and deferred wider forms.
- [x] Write an independently authored feature spec with MyLite Lemon snippets.
- [x] Add MySQL 8.4.9 expectation artifact.
- [ ] Add parser/AST support for the narrow `DATE_ADD` interval-second shape.
- [ ] Implement MyLite-owned scalar evaluator support without SQLite fork
  changes.
- [ ] Add fast C parser/runtime tests and CTest registration.
- [x] Update compatibility docs with limited wording.
- [ ] Run MySQL expectation script, focused CTests, `cmake --build --preset
  dev`, and `cmake --workflow --preset check`.
- [ ] Review final diff for MySQL evidence, scope control, SQL-mode behavior,
  temporal arithmetic correctness, no catalog/storage mutation, cleanup, and
  docs accuracy.
