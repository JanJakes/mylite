# Baseline Transaction Lifecycle Tasks

- [x] Review current statement-atomic DDL/DML transaction wrappers and catalog
      mutation boundaries.
- [x] Verify MySQL 8.4.9 behavior for basic transaction control, nested start,
      statement errors, DDL implicit commits, and disconnect rollback.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [ ] Add parser/AST support for the supported transaction-control statements.
- [ ] Add connection-local user transaction state and close-time rollback.
- [ ] Add internal statement transaction/savepoint helpers.
- [ ] Move current supported DML writes onto the statement transaction helper.
- [ ] Apply implicit user-transaction commit before current supported DDL
      statements.
- [ ] Add focused C runtime tests for commit, rollback, nested start, DDL
      implicit commit, statement-error savepoints, file-backed persistence, and
      close-time rollback.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run the feature MySQL expectation script.
- [ ] Run targeted parser/runtime CTest entries.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, commit atomically, push `main`, and run a
      subagent release-gate review.
