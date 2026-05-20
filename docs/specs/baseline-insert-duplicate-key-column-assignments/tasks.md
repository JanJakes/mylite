# Baseline INSERT Duplicate Key-Column Assignments Tasks

- [x] Confirm official MySQL 8.4 `INSERT ... ON DUPLICATE KEY UPDATE`
      documentation and MySQL 8.4.9 runtime behavior for valid and conflicting
      key-column duplicate assignments.
- [x] Specify the independently authored MyLite feature scope, diagnostics,
      ownership boundaries, generated SQL shape, and deferred cases.
- [x] Add a MySQL expectation script for supported key-column assignments and
      intentionally deferred auto-increment / parent-FK assignment behavior.
- [x] Extend duplicate-update planning to admit non-auto-increment key-column
      assignment targets while keeping duplicate-target and qualified-target
      guardrails.
- [x] Reject key-column assignments that modify referenced parent foreign-key
      parts until ODKU referential actions are implemented.
- [x] Detect second-order primary and unique key conflicts from the projected
      duplicate row before physical mutation and map them to MySQL-compatible
      duplicate-key diagnostics.
- [x] Preserve existing value conversion, warning, affected-row,
      auto-increment-attempt advancement, transaction, persistence, and
      file-format behavior.
- [x] Add runtime C coverage for valid reassignment, no-op `VALUES()` key
      assignment, composite/prefix unique keys, second-order conflicts,
      rollback, persistence, independent handles, and unsupported shapes.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused parser/runtime tests, the MySQL expectation script, and
      `cmake --workflow --preset check`.
- [x] Review with a subagent, amend if needed, commit, and push `main`.
