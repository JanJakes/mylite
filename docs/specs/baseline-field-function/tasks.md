# Baseline FIELD Function Tasks

- [x] Verify `FIELD()` behavior against MySQL 8.4.9 for arity, `NULL`,
  no-match, string, integer, default collation, `DUAL`, `DO`, row labels, and
  table-backed projection.
- [x] Specify the narrow supported grammar, runtime domain classification,
  diagnostics, ownership boundaries, generated SQLite shape, and deferred
  surfaces.
- [ ] Add parser/AST support for `FIELD()` with MySQL-compatible argument-count
  diagnostics.
- [ ] Extend row-scalar planning and SQLite SQL generation for flat
  all-string/all-integer `FIELD()` expressions.
- [ ] Add MySQL-runtime expectation script and fast C parser/runtime tests.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run focused tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review, commit, and push to `origin/main`.
