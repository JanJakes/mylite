# Baseline lower_case_file_system System Variable Tasks

- [x] Verify MySQL 8.4.9 behavior for scalar reads, `SHOW VARIABLES`, and
  read-only `SET` diagnostics.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add MySQL expectation script for the admitted user-visible subset.
- [x] Add `lower_case_file_system` to the runtime system-variable registry.
- [x] Return fixed scalar value `0` and `SHOW VARIABLES` value `OFF`.
- [x] Preserve MySQL's global-only scalar diagnostics while keeping the row
  visible in session/local `SHOW VARIABLES`.
- [x] Emit read-only diagnostics for admitted `SET` forms.
- [x] Extend runtime tests for scalar reads, `SHOW` filtering, and diagnostics.
- [x] Update compatibility matrix and detail docs.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [ ] Review, commit, and push to `origin/main`.
