# Baseline max_allowed_packet System Variable Tasks

- [x] Verify MySQL 8.4.9 behavior for scalar reads, `SHOW VARIABLES`, session
  read-only assignment diagnostics, and global assignment behavior.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add MySQL expectation script for the admitted user-visible subset.
- [x] Add `max_allowed_packet` to the runtime system-variable registry.
- [x] Return fixed scalar and `SHOW VARIABLES` value `67108864`.
- [x] Preserve MySQL's session-read-only diagnostics for non-global assignment
  targets.
- [x] Accept only exact no-op global assignments and reject value-changing
  assignments deterministically.
- [x] Extend runtime tests for scalar reads, `SHOW` filtering, no-op global
  assignment, and diagnostics.
- [x] Update compatibility matrix and detail docs.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [x] Review, commit, and push to `origin/main`.
