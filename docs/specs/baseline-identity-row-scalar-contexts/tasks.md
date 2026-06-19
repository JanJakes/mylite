# Baseline Identity Row-Scalar Contexts Tasks

## Implementation

- [x] Admit `USER()` through the row-scalar predicate expression and
  comparison-value grammar paths.
- [x] Admit `CURRENT_USER()` and bare `CURRENT_USER` through those paths.
- [x] Admit no-whitespace `SESSION_USER()` and `SYSTEM_USER()` through those
  paths without changing whitespace-sensitive parsing.
- [x] Admit `CURRENT_ROLE()` through those paths.
- [x] Route the identity functions through the existing row-scalar
  session-value constant planning path.
- [x] Admit the identity-function AST kinds in row-scalar context-expression
  detection for predicate and order-expression planning.

## Tests

- [x] Add C coverage for table-backed identity and current-role projection.
- [x] Add C coverage for identity-function `WHERE` comparisons and `ORDER BY`.
- [x] Add parser coverage for the identity-function comparison operands.
- [x] Add MySQL 8.4.9 expectation coverage for labels, values, ordering,
  warning behavior, and following `ROW_COUNT()`.

## Documentation

- [x] Add the feature specification.
- [x] Update the system-function compatibility tables.
- [x] Keep authentication, account, role-state, privilege, stored-program, and
  `IGNORE_SPACE` semantics tracked outside this row-scalar slice.
