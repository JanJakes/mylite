# Baseline ALTER TABLE Default Charset And Collation Tasks

Add a narrow fixed table-default charset/collation ALTER slice for existing
persistent base tables.

## Checklist

- [x] Verify MySQL 8.4.9 behavior for supported forms, quoted names,
  diagnostics, result shape, warning count, row count, and row preservation.
- [x] Specify syntax, runtime semantics, diagnostics, architecture boundaries,
  SQLite non-involvement, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST kinds/names, and parser helpers for fixed
  `ALTER TABLE` charset/collation table options.
- [ ] Add runtime execution that resolves the target descriptor, validates the
  fixed `utf8mb4` / `utf8mb4_0900_ai_ci` options, and returns an empty non-row
  result without catalog or storage mutation.
- [ ] Add C parser/runtime tests for success paths, diagnostics, result shape,
  preamble preservation, reopen behavior, and independent handles.
- [ ] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, fixed no-op
  semantics, diagnostics, docs accuracy, cleanup, and test relevance.
