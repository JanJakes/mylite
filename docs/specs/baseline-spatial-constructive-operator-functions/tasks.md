# Baseline Spatial Constructive Operator Functions Tasks

- [x] Verify MySQL 8.4.9 behavior for strategy bytes, NULL handling, arity,
      point/MultiPoint set operations, buffer zero-distance identity, and
      transform diagnostics.
- [x] Specify the bounded MyLite baseline and remaining constructive topology
      gaps.
- [x] Register remaining spatial operator names in the runtime dispatcher.
- [x] Implement `ST_Buffer_Strategy()` strategy byte strings and diagnostics.
- [x] Implement `ST_Buffer()` zero-distance identity behavior.
- [x] Implement Point/MultiPoint `ST_Difference()`, `ST_Intersection()`,
      `ST_SymDifference()`, and `ST_Union()`.
- [x] Implement identity-only `ST_Transform()` with MySQL diagnostics.
- [x] Add MySQL-runtime expectation script and focused runtime tests.
- [x] Update compatibility documentation.
- [x] Run focused tests and static checks.
- [x] Run release-gate review and pre-commit checks.
