# Baseline Insert Select Dual Scalar Where Tasks

- [x] Read current architecture, compatibility docs, existing
  `baseline-insert-select-dual-source` spec, parser, and row-scalar runtime
  paths.
- [x] Research official MySQL 8.4 `INSERT ... SELECT`, `SELECT`, and
  expression documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `FROM DUAL WHERE` scalar filters,
  zero-row sources, diagnostic ordering, affected rows, and warning counts.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, generated SQLite shape, diagnostics, and test
  plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Add planner/runtime support for tableless/`DUAL` scalar-literal filters.
- [x] Add focused parser/runtime tests.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
