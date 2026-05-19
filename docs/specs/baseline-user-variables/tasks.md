# Baseline User Variables Tasks

- [x] Verify MySQL 8.4.9 behavior for uninitialized variables, name folding,
  quoted names, assignment lists, system-variable save/restore, atomic failure,
  and name-length diagnostics.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add parser/AST support for user-variable expressions, assignment lists,
  and `:=` user-variable assignment.
- [x] Add handle-local user-variable storage with zero-init-safe cleanup.
- [x] Evaluate user variables in the existing scalar-expression path.
- [x] Apply mixed `SET` assignment lists atomically against user and supported
  system variables.
- [x] Add MySQL-runtime expectation script.
- [x] Add parser/runtime C tests and CMake registration.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused parser/runtime tests, MySQL expectation script, and the full
  check workflow.
- [x] Review, commit, and push.
