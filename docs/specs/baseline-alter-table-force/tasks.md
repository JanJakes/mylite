# Baseline ALTER TABLE FORCE Tasks

Add a narrow physical rebuild slice for existing persistent base tables.

## Checklist

- [x] Verify MySQL 8.4.9 behavior for supported forms, diagnostics, result
  shape, warning count, row count, and row preservation.
- [x] Specify syntax, runtime semantics, diagnostics, architecture boundaries,
  SQLite physical rebuild behavior, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST kind/name, and parser helper for
  `ALTER TABLE ... FORCE`.
- [ ] Add runtime planning and execution that resolves target descriptors,
  performs a SQLite-side rebuild, preserves rows/descriptors, and reports
  zero affected rows without catalog mutation.
- [ ] Add C parser/runtime tests for success paths, diagnostics, result shape,
  row preservation, preamble preservation, reopen behavior, and independent
  handles.
- [ ] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, rebuild semantics,
  diagnostics, docs accuracy, cleanup, performance, and test relevance.
