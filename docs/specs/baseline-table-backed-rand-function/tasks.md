# Baseline Table-Backed RAND Function Tasks

- [x] Read current project guidance, engineering standards, compatibility
  matrix, RAND tests, parser, row-scalar SELECT, ORDER BY, SQLite registration,
  and result-metadata paths.
- [x] Verify official MySQL 8.4 `RAND([N])` documentation and MySQL 8.4.9
  runtime behavior for table-backed projection, seeded sequences, duplicate
  seeded expressions, and random ordering.
- [x] Write the independently authored feature specification with grammar
  snippet, semantics, diagnostics, architecture boundaries, and test plan.
- [x] Add MySQL-runtime expectation coverage for table-backed RAND projection
  and ORDER BY RAND.
- [x] Add MySQL-runtime expectation coverage for descriptor-seeded RAND
  projection, limited RAND WHERE comparisons, and string-target RAND DML values.
- [x] Add a private MyLite SQLite RAND scalar module with expression-scoped
  seeded state and no SQLite fork changes.
- [x] Extend row-scalar planning, SQL generation, parameter binding, result
  metadata, and SELECT ORDER BY planning for the admitted RAND subset.
- [x] Extend parser and DML conversion support for the admitted WordPress-style
  RAND predicate and value forms.
- [x] Add parser/runtime tests for successful and rejected forms.
- [x] Update `COMPATIBILITY.md` and numeric/math compatibility details.
- [x] Run focused parser/runtime CTests and MySQL expectation scripts.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the feature, fix issues, commit, and push to remote `main`.
