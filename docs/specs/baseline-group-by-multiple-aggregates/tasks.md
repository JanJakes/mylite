# Baseline GROUP BY Multiple Aggregates Tasks

- [x] Read project architecture, compatibility, grouped aggregate specs, runtime
  code, parser support, and existing grouped aggregate tests.
- [x] Research official MySQL 8.4 `SELECT`, grouped-query handling, and
  aggregate function documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for multiple grouped aggregate
  select items, selected aggregate `HAVING`, aggregate-alias ordering, row
  count, and warning count.
- [x] Specify the supported MyLite grammar subset, architecture boundaries,
  generated SQLite shape, result metadata, diagnostics, and deferred surface.
- [x] Add MySQL-runtime expectation script for the new slice.
- [x] Extend grouped aggregate planning from one aggregate item to a bounded
  aggregate item list.
- [x] Extend generated SQLite select-list, `HAVING`, `ORDER BY`, binding, and
  public row formatting for multiple aggregate result items.
- [x] Extend fast C runtime tests and compatibility documentation.
- [x] Run MySQL expectation script, targeted parser/runtime tests, build, and
  full check workflow.
- [x] Review, amend if needed, commit, and push.
