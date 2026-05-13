# Baseline Transaction Lifecycle Tasks

- [x] Review current statement-atomic DDL/DML transaction wrappers and catalog
      mutation boundaries.
- [x] Verify MySQL 8.4.9 behavior for basic transaction control, nested start,
      statement errors, DDL implicit commits, and disconnect rollback.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [x] Add parser/AST support for the supported transaction-control statements.
- [x] Add connection-local user transaction state and close-time rollback.
- [x] Add internal statement transaction/savepoint helpers.
- [x] Move current supported DML writes onto the statement transaction helper.
- [x] Apply implicit user-transaction commit before current supported DDL
      statements.
- [x] Add focused C runtime tests for commit, rollback, nested start, DDL
      implicit commit, statement-error savepoints, file-backed persistence, and
      close-time rollback.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run the feature MySQL expectation script.
- [x] Run targeted parser/runtime CTest entries.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, push `main`, and run a
      subagent release-gate review.
