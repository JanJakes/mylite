# Baseline INSERT Duplicate Multiple Enforced Keys Tasks

- [x] Confirm official MySQL 8.4 `INSERT ... ON DUPLICATE KEY UPDATE`
      documentation for multiple unique indexes and affected-row behavior.
- [x] Verify MySQL 8.4.9 runtime behavior for primary-plus-unique,
      two-unique, composite-plus-unique, `NULL` unique parts, no-op duplicate
      updates, and generated auto-increment consumption.
- [x] Add a MySQL expectation script for supported multi-enforced-key behavior
      and intentionally deferred key-column assignment semantics.
- [x] Extend duplicate-update planning to admit multiple enforced primary or
      unique key descriptors with descriptor key parts.
- [x] Reject duplicate assignment targets that participate in any enforced key.
- [x] Preserve descriptor-built conflict selection, generated physical SQL,
      bound parameters, affected-row semantics, warning semantics, statement
      atomicity, and file-format safety.
- [x] Add runtime C coverage for primary-plus-unique, two-unique,
      composite-plus-unique, `NULL` unique parts, no-op branches,
      auto-increment, diagnostics, persistence, and regression behavior.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused runtime tests, the MySQL expectation script, and
      `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend if needed, commit, and push `main`.
