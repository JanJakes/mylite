# Baseline Transaction System Variables Tasks

- [x] Verify MySQL 8.4.9 behavior for scalar reads, `SHOW VARIABLES`, session
  assignment, next-transaction `@@` assignment, active-transaction diagnostics,
  global assignment behavior, and invalid values.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add MySQL expectation script for the admitted user-visible subset.
- [x] Add `transaction_isolation` and `transaction_read_only` to the runtime
  system-variable registry.
- [x] Return scalar and `SHOW VARIABLES` values from transaction session state
  or fixed global defaults.
- [x] Support direct session/default/local assignments and direct `@@`
  next-transaction assignments.
- [x] Accept only exact no-op global assignments and reject value-changing
  global assignments deterministically.
- [x] Extend parser support for direct bare `SERIALIZABLE` assignment values.
- [x] Extend runtime tests for scalar reads, `SHOW` filtering, assignment
  behavior, read-only effects, diagnostics, persistence boundaries, and
  independent handles.
- [x] Update compatibility matrix and detail docs.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [x] Review, commit, and push to `origin/main`.
