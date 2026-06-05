# Baseline Row Control-Flow Functions Tasks

- [x] Create independently authored feature spec.
- [x] Verify row-backed control-flow projection behavior against MySQL 8.4.9.
- [x] Add MySQL-runtime expectation artifact.
- [x] Add row-scalar planner support for `IF()`, `IFNULL()`, `COALESCE()`,
  `NULLIF()`, and `ISNULL()`.
- [x] Lower supported expressions to quoted, parameterized SQLite SQL without
  row-set materialization.
- [x] Add fast runtime tests for supported projections and deterministic
      unsupported forms.
- [x] Extend the supported row envelope to searched `CASE` order keys and add
      MySQL/runtime coverage.
- [x] Support `AND`/`OR` composition over searched `CASE` order-key `LIKE`
      conditions for WordPress search ranking.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `main`.
