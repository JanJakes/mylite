# Baseline Savepoint Lifecycle Tasks

- [x] Review existing transaction-control parser/runtime architecture and
      internal statement savepoint behavior.
- [x] Verify MySQL 8.4.9 behavior for savepoint creation, replacement,
      rollback-to, release, missing-savepoint diagnostics, cleanup across
      transaction boundaries, DDL implicit commit, and quoted/case-insensitive
      names.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [ ] Add parser/AST support for supported savepoint forms.
- [ ] Add connection-local savepoint registry and cleanup helpers.
- [ ] Add runtime handlers for `SAVEPOINT`, `ROLLBACK TO [SAVEPOINT]`, and
      `RELEASE SAVEPOINT`.
- [ ] Preserve MySQL duplicate-name replacement semantics instead of SQLite
      duplicate-name behavior.
- [ ] Preserve existing internal statement savepoint atomicity inside active
      user transactions.
- [ ] Add parser and runtime C tests for supported forms, no-op autocommit
      behavior, missing-savepoint diagnostics, duplicate replacement,
      rollback-to/release stack behavior, DDL cleanup, statement-error
      preservation, file-backed persistence, independent handles, and
      zero-initialized cleanup.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run the feature MySQL expectation script.
- [ ] Run targeted parser/runtime CTest entries.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, commit atomically, push `main`, and run a
      subagent release-gate review.
