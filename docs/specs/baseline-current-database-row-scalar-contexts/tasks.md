# Baseline Current Database Row-Scalar Contexts Tasks

## Implementation

- [x] Route `DATABASE()` and `SCHEMA()` through source-backed row-scalar SELECT
  detection.
- [x] Admit `DATABASE()` and `SCHEMA()` as comparison-left predicate row-scalar
  grammar expressions.
- [x] Reuse existing row-scalar constant value planning and SQLite parameter
  binding.
- [x] Admit the functions in row-scalar predicate and order-expression
  detection.

## Tests

- [x] Add C coverage for table-backed current-schema projection.
- [x] Add C coverage for `WHERE DATABASE() = ...` and `ORDER BY SCHEMA()`.
- [x] Add C coverage for `INSERT ... SELECT ... FROM DUAL WHERE DATABASE() = ...`.
- [x] Add MySQL 8.4.9 expectation coverage for projection, predicate, ordering,
  labels, values, and insert-select side effects.

## Documentation

- [x] Add the feature specification.
- [x] Update the system-function compatibility tables.
- [x] Record the remaining stored-routine binding gap as outside current
  stored-program support.
