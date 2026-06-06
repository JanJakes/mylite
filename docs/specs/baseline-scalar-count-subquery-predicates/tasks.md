# Baseline Scalar COUNT Subquery Predicates Tasks

- [x] Specify the supported WordPress scalar `COUNT()` subquery comparison
  subset and unsupported forms.
- [x] Add MySQL 8.4.9 runtime expectations for correlated `COUNT(1)` scalar
  subquery comparisons.
- [x] Admit the parser grammar for parenthesized scalar subquery comparisons.
- [x] Plan supported `COUNT(*)` / `COUNT(literal)` scalar subquery comparisons
  through the existing correlated subquery source and predicate machinery.
- [x] Emit SQL and bind parameters in subquery-then-comparison order.
- [x] Add focused MyLite runtime coverage.
- [x] Run focused C, MySQL expectation, and WordPress PHPUnit checks.
- [ ] Run broader release checks.
