# Baseline Insert Select Dual Source Tasks

- [x] Read current architecture, compatibility docs, insert/select specs, parser
  and runtime code, and SQLite policy.
- [x] Research official MySQL 8.4 `INSERT ... SELECT` and `SELECT ... FROM
  DUAL` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for row-scalar `INSERT ... SELECT`,
  `FROM DUAL`, `WHERE EXISTS`, `WHERE NOT EXISTS`, zero-row sources, omitted
  columns, auto-increment, duplicate keys, and diagnostics.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, generated SQLite shape, diagnostics, and test
  plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend parser support for `SELECT ... FROM DUAL WHERE ...`.
- [x] Add planner/runtime support for row-scalar `INSERT ... SELECT` sources.
- [x] Add focused parser/runtime tests and register any new test binary.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
