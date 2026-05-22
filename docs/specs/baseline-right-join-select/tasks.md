# Baseline RIGHT JOIN SELECT Tasks

- [x] Read current architecture, compatibility docs, existing join specs,
  parser grammar, planner/runtime join code, tests, and SQLite fork policy.
- [x] Research official MySQL 8.4 `SELECT`, `JOIN`, and outer-join
  simplification documentation for right outer joins.
- [x] Probe MySQL 8.4.9 runtime behavior for two-source right joins, column
  order, null extension, aliasing, ambiguity, diagnostics, warnings, and
  `ROW_COUNT()`.
- [x] Write the independently authored feature spec with grammar snippets,
  ownership boundaries, unsupported surfaces, diagnostics, performance notes,
  and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend parser support for `RIGHT [OUTER] JOIN`.
- [x] Extend descriptor-backed joined `SELECT` planning and SQL generation.
- [x] Add parser and focused runtime C tests.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
