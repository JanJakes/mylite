# Baseline CONNECTION_ID Row-Scalar Contexts Tasks

## Implementation

- [x] Admit `CONNECTION_ID()` through the shared statement-context row-scalar
  parser helper.
- [x] Preserve nonreserved `CONNECTION_ID` identifier behavior through parser
  fallback and bare-expression identifier parsing.
- [x] Route `CONNECTION_ID()` through the existing row-scalar session-value
  constant planning path.
- [x] Admit `CONNECTION_ID()` in row-scalar context-expression detection for
  predicate and order-expression planning.

## Tests

- [x] Add C coverage for table-backed `CONNECTION_ID()` projection.
- [x] Add C coverage for `WHERE CONNECTION_ID() = CONNECTION_ID()` and
  `ORDER BY CONNECTION_ID()`.
- [x] Add parser coverage for `CONNECTION_ID()` comparison operands.
- [x] Add MySQL 8.4.9 expectation coverage for values, ordering, and warning
  behavior.

## Documentation

- [x] Add the feature specification.
- [x] Update the system-function compatibility tables.
- [x] Keep server-thread, process-list, Performance Schema, protocol, and
  `pseudo_thread_id` identity semantics tracked outside this function row.
