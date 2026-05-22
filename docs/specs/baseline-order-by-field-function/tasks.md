# Baseline ORDER BY FIELD Function Tasks

- [x] Verify `ORDER BY FIELD()` behavior against MySQL 8.4.9 for ascending,
  descending, `WHERE`, `LIMIT`, alias-qualified columns, schema-qualified
  columns, `NULL` and no-match rank `0`, and unknown-column diagnostics.
- [x] Write the independently authored feature spec with MyLite grammar subset,
  ownership boundaries, diagnostics, SQLite handling, and test plan.
- [x] Add MySQL-runtime expectation script for the supported and deferred
  user-visible behavior.
- [x] Extend the order planner to support exactly one hidden `FIELD()` order
  expression for supported single-table `SELECT` paths.
- [x] Reuse existing descriptor-driven `FIELD()` argument planning,
  expression-SQL generation, and parameter binding with `order clause`
  diagnostics.
- [x] Keep `UPDATE`, `DELETE`, `TABLE`, grouped, distinct, joined, compound,
  and insert-source paths rejected for this expression-ordering slice.
- [x] Add fast C runtime and parser tests.
- [x] Update compatibility docs with limited support wording.
- [x] Run focused CTest entries, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the diff for MySQL behavior, descriptor authority, parameter
  binding order, no row materialization, ABI stability, docs accuracy, and
  scope control.
