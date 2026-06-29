# Baseline `GROUP BY ... WITH ROLLUP` Tasks

- [x] Verify MySQL 8.4.9 single-key rollup behavior with ordinary `NULL` groups.
- [x] Verify rollup aggregate totals for `COUNT`, `SUM`, `MIN`, `MAX`, and `AVG`.
- [x] Verify source `WHERE` filtering and empty filtered-source behavior.
- [x] Specify the bounded executable envelope and rejected combinations.
- [x] Add a MySQL-runtime expectation script.
- [x] Implement the grouped aggregate planner/runtime support.
- [x] Add runtime tests for supported rows and explicit diagnostics.
- [x] Update compatibility documentation.
- [x] Run full release-gate verification before commit.
