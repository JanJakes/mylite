# Parser Corpus DDL Default And Order Residuals Tasks

- [x] Verify MySQL 8.4.9 behavior for repeated defaults, `FLOAT(10.3)`,
  comma-tail `ALTER TABLE ... ORDER BY`, and bare partition operations.
- [x] Specify executable repeated-default and `FLOAT(10.3)` behavior plus
  unsupported placeholder behavior for reorder and partition DDL.
- [x] Implement parser and runtime changes.
- [x] Add parser/runtime tests and MySQL expectation coverage.
- [x] Rerun focused tests, parser corpus benchmark, formatting, static checks,
  and release-gate review before commit.
