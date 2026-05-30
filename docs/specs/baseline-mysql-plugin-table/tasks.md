# Baseline mysql.plugin Table Tasks

- [x] Confirm current branch, worktree state, project rules, and relevant
  schema-catalog compatibility docs.
- [x] Verify `mysql.plugin` behavior against MySQL 8.4.9, including
  connection-control registry rows, columns, primary key, constraints, and
  table status.
- [x] Specify the independently authored MyLite scope and compatibility limits.
- [x] Add MySQL expectation coverage for plugin table rows, metadata, primary
  key catalogs, and InnoDB table status.
- [x] Implement the static `mysql.plugin` table definition and rows.
- [x] Extend C runtime coverage and register the focused CTest target.
- [x] Update `COMPATIBILITY.md`, compatibility guides, and shared metadata
  specs.
- [x] Run focused tests, MySQL expectation script, whitespace checks, tidy, and
  the full check workflow.
- [x] Review the completed slice, fix findings, commit, and push.
