# Baseline JSON Contains Functions Tasks

- [x] Review existing JSON type, validation, extraction, introspection,
  construction, row-scalar, predicate, and SQLite callback architecture.
- [x] Verify MySQL 8.4.9 behavior for the admitted `JSON_CONTAINS()` and
  `JSON_CONTAINS_PATH()` subset.
- [x] Write the feature specification and MySQL expectation artifact.
- [x] Add parser/AST support for `JSON_CONTAINS()` and
  `JSON_CONTAINS_PATH()` plus native argument-count diagnostics.
- [x] Add MyLite JSON runtime containment and path-existence helpers.
- [x] Register SQLite scalar callbacks and map callback diagnostics.
- [x] Add scalar, row-scalar projection, and predicate planning/execution.
- [x] Add C runtime tests and CTest registration.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run MySQL expectations, focused tests, full build, workflow check, and
  self-review.
