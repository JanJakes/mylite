# Baseline STRAIGHT_JOIN No-Op Tasks

- [x] Read current architecture, compatibility docs, existing join specs,
      parser grammar, planner/runtime join code, tests, and SQLite fork policy.
- [x] Research official MySQL 8.4 join and joined DML documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `STRAIGHT_JOIN` in `SELECT`,
      joined `DELETE`, joined `UPDATE`, wildcard projection, no-`ON`
      Cartesian joins, string collation, warnings, and row counts.
- [x] Write the independently authored feature spec with grammar snippets,
      ownership boundaries, unsupported surfaces, diagnostics, performance
      notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend parser support for `STRAIGHT_JOIN` as an inner join operator.
- [x] Add parser and focused runtime C tests.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority,
      performance, cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
