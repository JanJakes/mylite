# Baseline UPDATE Scalar Subquery Assignment Tasks

- [x] Verify MySQL 8.4.9 behavior for scalar subquery assignment, cardinality,
      no-match evaluation, affected rows, warnings, and same-table
      restrictions.
- [x] Specify the narrow MyLite grammar and runtime subset before code changes.
- [x] Add parser support for scalar subquery `update_value` nodes.
- [x] Add descriptor-driven planning for one table-backed scalar assignment
      source column.
- [x] Add matched-row-only scalar source execution with 0-row `NULL`, 1-row
      value materialization, and 2-row `1242` cardinality detection.
- [x] Add target conversion/validation for compatible descriptor source values.
- [x] Add runtime and parser tests, including MySQL expectation artifacts.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectation script, and `cmake --workflow --preset
      check`.
- [x] Review the feature and fix findings.
- [ ] Commit and push `main`.
