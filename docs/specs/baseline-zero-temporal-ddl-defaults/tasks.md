# Baseline Zero Temporal DDL Defaults Tasks

- [x] Verify MySQL 8.4.9 behavior for ALTER temporal defaults and record a
      focused expectation script.
- [x] Add runtime tests for `ADD COLUMN`, `ALTER COLUMN SET DEFAULT`,
      `MODIFY COLUMN`, and `CHANGE COLUMN` zero temporal default behavior.
- [x] Implement any missing descriptor default conversion or warning-count
      behavior.
- [x] Update compatibility docs for the exact supported DDL default subset.
- [x] Run focused build/tests, the new MySQL expectation script, and the full
      check workflow.
- [x] Review the implementation, fix findings, commit atomically, and push.
