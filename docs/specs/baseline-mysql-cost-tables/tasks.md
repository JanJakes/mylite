# Baseline mysql Cost Tables Tasks

- [x] Confirm current branch, worktree state, project rules, and relevant
  schema-catalog compatibility docs.
- [x] Verify `mysql.server_cost` and `mysql.engine_cost` behavior against
  MySQL 8.4.9, including rows, columns, generated expressions, primary keys,
  constraints, and table status.
- [x] Specify the independently authored MyLite scope and compatibility limits.
- [x] Add MySQL expectation coverage for cost table rows, metadata, primary-key
  catalogs, generated expressions, and InnoDB table status.
- [x] Implement the static `mysql.server_cost` and `mysql.engine_cost` table
  definitions and rows.
- [x] Extend C runtime coverage and register the focused CTest target.
- [x] Update `COMPATIBILITY.md`, compatibility guides, and shared metadata
  specs.
- [x] Run focused tests, MySQL expectation script, whitespace checks, tidy, and
  the full check workflow.
- [x] Review the completed slice and fix findings.
- [x] Commit and push.
