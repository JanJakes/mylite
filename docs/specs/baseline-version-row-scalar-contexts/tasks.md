# Baseline VERSION Row-Scalar Contexts Tasks

## Implementation

- [x] Admit `VERSION()` as a comparison-left predicate row-scalar expression.
- [x] Admit `VERSION()` as a predicate comparison value.
- [x] Route `VERSION()` through the existing row-scalar session-value constant
  planning path.
- [x] Admit `VERSION()` in row-scalar context-expression detection for
  predicate and order-expression planning.

## Tests

- [x] Add C coverage for table-backed `VERSION()` projection.
- [x] Add C coverage for `WHERE VERSION() = VERSION()` and
  `ORDER BY VERSION()`.
- [x] Add parser coverage for `VERSION()` comparison operands.
- [x] Add MySQL 8.4.9 expectation coverage for labels, values, ordering, and
  warning behavior.

## Documentation

- [x] Add the feature specification.
- [x] Update the system-function compatibility tables.
- [x] Keep protocol handshake and configurable server-version identity tracked
  outside the SQL-visible `VERSION()` row.
