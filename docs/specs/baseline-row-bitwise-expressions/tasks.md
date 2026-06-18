# Baseline Row Bitwise Expressions Tasks

- [x] Define the supported table-backed numeric bitwise expression subset.
- [x] Add MySQL 8.4.9 runtime expectation probe.
- [x] Evaluate parser admission for row predicate and order-by bitwise
  expressions; defer direct predicate/order-key grammar until the row expression
  grammar can be broadened without Lemon conflicts.
- [x] Add row-scalar planning, SQL generation, and parameter binding support for
  projection expressions.
- [x] Register MyLite SQLite scalar UDFs for row bitwise operations.
- [x] Add focused runtime tests.
- [x] Update compatibility documentation.
- [x] Run focused verification, release-gate review, full check workflow, commit,
  and push.
- [x] Admit and verify parenthesized row bitwise comparison predicates.
- [x] Admit and verify direct row bitwise `ORDER BY` keys.
- [x] Update compatibility documentation for predicate/order support.
- [x] Run focused verification, release-gate review, full check workflow, commit,
  and push the predicate/order extension.
