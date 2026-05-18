# Baseline SHOW FULL TABLES Tasks

- [x] Verify MySQL 8.4.9 behavior for `SHOW FULL TABLES`, schema forms,
  `LIKE`, headers, row-count state, missing default schema, unknown schema, and
  base-table type values.
- [x] Specify ownership boundaries, syntax, semantics, diagnostics, test
  expectations, and compatibility documentation scope.
- [x] Implement parser/AST support for the `FULL` modifier on `SHOW TABLES`.
- [x] Implement runtime result shape and `BASE TABLE` row values.
- [x] Add MySQL expectation and fast C coverage.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run the MySQL expectation script, focused CTest entries, and full
  `cmake --workflow --preset check`.
