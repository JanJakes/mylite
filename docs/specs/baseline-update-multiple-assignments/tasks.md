# Baseline UPDATE Multiple Assignments Tasks

- [x] Confirm official MySQL 8.4 `UPDATE` documentation and MySQL 8.4.9 runtime
      behavior for multiple assignment changed-row semantics, no-match
      conversion skipping, duplicate target behavior, and ordered-limited
      updates.
- [x] Add MySQL expectation script for the supported user-visible subset.
- [x] Extend the update planner to resolve and validate multiple distinct
      unqualified non-key assignment targets.
- [x] Reuse existing single-assignment constant conversion for each assignment
      only after matched-row checks.
- [x] Generate one descriptor-driven SQLite `UPDATE` with multiple assignment
      parameters and an `OR` changed-row filter.
- [x] Preserve automatic `ON UPDATE CURRENT_TIMESTAMP` behavior when explicit
      assignments change a row.
- [x] Add runtime C coverage for success, diagnostics, persistence,
      file-format safety, independent handles, and unsupported multi-assignment
      shapes.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused parser/runtime tests, the MySQL expectation script, and
      `cmake --workflow --preset check`.
- [x] Commit, push `main`, and run a review subagent before moving to the next
      baseline slice.
