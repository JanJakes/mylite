# Baseline Row Arithmetic Predicate Values Tasks

- [x] Verify MySQL 8.4.9 behavior for row arithmetic predicate values in
      comparison RHS, `BETWEEN` bounds, and `IN` list entries.
- [x] Document supported syntax, runtime scope, SQLite integration approach,
      diagnostics, and deferred expression surfaces.
- [x] Add parser support for row arithmetic comparison RHS and range operands.
- [x] Reuse the existing row-scalar value planner for arithmetic predicate
      values.
- [x] Add MyLite runtime coverage for comparison, range, negated range, and
      membership value cases.
- [x] Add MySQL expectation coverage for the new predicate value cases.
- [x] Update compatibility docs and related row arithmetic specs.
- [x] Run focused verification, release-gate review, full checks, commit, and
      push.
