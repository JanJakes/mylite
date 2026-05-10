# Baseline CREATE TABLE LIKE Tasks

Add a narrow descriptor-clone slice for persistent base tables.

## Checklist

- [x] Verify MySQL 8.4.9 behavior for supported forms, diagnostics, result
  shape, warning count, row count, empty clone behavior, and source validation
  order.
- [x] Specify syntax, descriptor cloning, diagnostics, architecture
  boundaries, physical SQLite handling, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST kind/name, and parser helper for
  `CREATE TABLE ... LIKE`.
- [ ] Add runtime planning and execution that resolves source/target
  descriptors, clones descriptor columns, creates an empty physical table, and
  reports MySQL-compatible result metadata.
- [ ] Preserve cloned visibility/default metadata and reject unsupported
  future descriptor types before mutation.
- [ ] Add C parser/runtime tests for success paths, diagnostics, result shape,
  descriptor cloning, source validation order, preamble preservation, reopen
  behavior, and independent handles.
- [ ] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, descriptor authority,
  physical table creation semantics, diagnostics, docs accuracy, cleanup,
  performance, and test relevance.
