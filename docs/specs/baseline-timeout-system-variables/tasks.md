# Baseline Timeout System Variables Tasks

- [x] Verify MySQL 8.4.9 behavior for scalar reads, `SHOW VARIABLES`, session
  assignment conversion, user-variable assignment conversion, clamp warnings,
  and global assignment behavior.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add MySQL expectation script for the admitted user-visible subset.
- [x] Add `wait_timeout` and `interactive_timeout` to session state and the
  runtime system-variable registry.
- [x] Return session/global scalar values and `SHOW VARIABLES` rows.
- [x] Implement session/local/no-scope/direct-`@@` assignment conversion,
  warning, and rollback behavior.
- [x] Accept only exact no-op global assignments and reject value-changing
  global assignments deterministically.
- [x] Extend runtime tests for scalar reads, `SHOW` filtering, assignments,
  diagnostics, warnings, independent handles, close/reopen reset, and
  preamble/catalog preservation.
- [x] Update compatibility matrix and detail docs.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [x] Review, commit, and push to `origin/main`.
