# Baseline Composite Unique Prefix Indexes Tasks

- [x] Review existing prefix index, unique prefix, and composite unique index
  specs and implementation paths.
- [x] Verify MySQL 8.4.9 behavior for composite unique prefix DDL, metadata,
  duplicate diagnostics, `NULL` tuples, `INSERT IGNORE`, and `UPDATE`.
- [x] Specify the supported grammar, descriptor ownership, validation,
  duplicate semantics, physical SQLite shape, result behavior, diagnostics, and
  compatibility documentation updates.
- [x] Remove the deliberate composite unique prefix rejection while preserving
  all existing prefix and composite unique validation.
- [x] Cover create-time, alter-time, standalone create-index, metadata, DML
  duplicate enforcement, `NULL` tuple, persistence, and diagnostics in fast C
  tests.
- [x] Add and run the MySQL 8.4.9 expectation script for this feature.
- [x] Update compatibility documentation for the exact admitted subset.
- [x] Run targeted parser/index/update tests, the MySQL expectation script,
  and `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, push `main`, and run a
  subagent release-gate review.
