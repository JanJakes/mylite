# Baseline mysql Log Tables Tasks

- [x] Confirm current branch, worktree state, project rules, and relevant
  schema-catalog compatibility docs.
- [x] Verify `mysql.general_log` and `mysql.slow_log` behavior against MySQL
  8.4.9, including direct row counts, columns, index absence, and table status.
- [x] Specify the independently authored MyLite scope and compatibility limits.
- [x] Add MySQL expectation coverage for log-table columns, empty reads,
  no-index/no-constraint metadata, and CSV table status.
- [x] Implement static MySQL-system-table definitions for both log tables.
- [x] Extend C runtime coverage and register the focused CTest target.
- [x] Update `COMPATIBILITY.md`, compatibility guides, and shared metadata
  specs.
- [x] Run focused tests, MySQL expectation script, whitespace checks, tidy, and
  the full check workflow.
- [x] Review the completed slice, fix findings, commit, and push.
