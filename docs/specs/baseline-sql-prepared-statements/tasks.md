# Baseline SQL Prepared Statements Tasks

- [x] Verify MySQL 8.4.9 behavior for statement lifecycle, case-insensitive
  names, replacement failure, row counts, unknown handlers, `DROP PREPARE`,
  source values, marker counts, invalid `USING`, direct `?`, invalid marker
  placement, nested prepared commands, and multiple-statement source text.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [ ] Add parser/AST support for `PREPARE`, `EXECUTE`, and
  `DEALLOCATE PREPARE` / `DROP PREPARE`.
- [ ] Add handle-local prepared statement storage with zero-init-safe cleanup.
- [ ] Preserve enough user-variable value kind to render `EXECUTE ... USING`
  parameters safely.
- [ ] Add marker counting, prepare-time validation, and execute-time expansion
  without admitting direct `?` SQL.
- [ ] Route expanded execution through existing `mylite_execute()` semantics.
- [ ] Add MySQL-runtime expectation script.
- [ ] Add parser/runtime C tests and CMake registration.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run focused parser/runtime tests, MySQL expectation script, and the full
  check workflow.
- [ ] Review, commit, and push.
