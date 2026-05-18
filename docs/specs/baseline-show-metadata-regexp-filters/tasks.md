# Baseline SHOW Metadata REGEXP Filters Tasks

- [x] Verify MySQL 8.4.9 behavior for `REGEXP`, `RLIKE`, `NOT REGEXP`, `NULL`
  cells, numeric metadata cells, invalid patterns, warning count, and
  `ROW_COUNT()` in the admitted `SHOW ... WHERE` contexts.
- [x] Specify ownership boundaries, syntax, semantics, diagnostics, test
  expectations, and compatibility documentation scope.
- [x] Implement metadata-cell regex evaluation for `SHOW COLUMNS`,
  `SHOW FULL COLUMNS`, `SHOW INDEX`, and `SHOW TABLE STATUS`.
- [x] Extend runtime C coverage for successful and diagnostic regex filters.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run the MySQL expectation script, focused CTest entries, and full
  `cmake --workflow --preset check`.
