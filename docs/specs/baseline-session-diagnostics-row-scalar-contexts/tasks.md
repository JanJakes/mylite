# Tasks

- [x] Record MySQL 8.4.9 row-backed `ROW_COUNT()`, `FOUND_ROWS()`, and
  zero-argument `LAST_INSERT_ID()` behavior.
- [x] Admit the read functions in row-scalar predicate grammar.
- [x] Route source-backed SELECTs containing these read functions through the
  row-scalar planner.
- [x] Plan statement-start session snapshots as bound literal values.
- [x] Emit `FOUND_ROWS()` deprecation warnings from projection, `WHERE`, and
  `ORDER BY` row-scalar contexts.
- [x] Add parser, runtime, and MySQL expectation coverage.
- [x] Update compatibility documentation.
- [ ] Design source-backed `LAST_INSERT_ID(expr)` side effects in a separate
  slice.
