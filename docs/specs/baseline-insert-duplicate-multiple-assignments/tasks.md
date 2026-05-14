# Baseline INSERT Duplicate Multiple Assignments Tasks

- [x] Confirm official MySQL 8.4 `INSERT ... ON DUPLICATE KEY UPDATE`
      documentation and MySQL 8.4.9 runtime behavior for multi-assignment
      affected rows, warning counts, no-key tables, duplicate targets, and
      duplicate-branch atomicity.
- [x] Add MySQL expectation script for the supported and intentionally deferred
      user-visible behavior.
- [ ] Extend duplicate-update planning to resolve and validate multiple
      distinct unqualified non-key assignment targets.
- [ ] Reuse existing duplicate-update literal and same-target `VALUES()`
      conversion for each assignment.
- [ ] Generate one descriptor-driven SQLite duplicate-row `UPDATE` with
      multiple assignment parameters and an `OR` changed-row filter.
- [ ] Preserve generated auto-increment, `LAST_INSERT_ID()`, affected-row, and
      warning behavior from the current ODKU path.
- [ ] Add runtime C coverage for success, warnings, diagnostics, atomicity,
      persistence, file-format safety, independent handles, and unsupported
      multi-assignment shapes.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run focused parser/runtime tests, the MySQL expectation script, and
      `cmake --workflow --preset check`.
- [ ] Commit, review with a subagent, amend if needed, and push `main`.
