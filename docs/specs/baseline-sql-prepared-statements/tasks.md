# Baseline SQL Prepared Statements Tasks

- [x] Verify MySQL 8.4.9 behavior for statement lifecycle, case-insensitive
  names, replacement failure, row counts, unknown handlers, `DROP PREPARE`,
  source values, marker counts, invalid `USING`, direct `?`, invalid marker
  placement, nested prepared commands, and multiple-statement source text.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add parser/AST support for `PREPARE`, `EXECUTE`, and
  `DEALLOCATE PREPARE` / `DROP PREPARE`.
- [x] Add handle-local prepared statement storage with zero-init-safe cleanup.
- [x] Preserve enough user-variable value kind to render `EXECUTE ... USING`
  parameters safely.
- [x] Add marker counting, prepare-time validation, and execute-time expansion
  without admitting direct `?` SQL.
- [x] Capture prepare-time lexer, default-database, character-set, and collation
  context while preserving execute-time parameter values, session-variable
  readback, and runtime validation modes.
- [x] Route expanded execution through existing statement execution semantics.
- [x] Add MySQL-runtime expectation script.
- [x] Add parser/runtime C tests and CMake registration.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused parser/runtime tests, MySQL expectation script, and the full
  check workflow.
- [x] Review, commit, and push.
