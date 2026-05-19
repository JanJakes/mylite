# Baseline GREATEST and LEAST Functions Tasks

- [x] Verify `GREATEST()` and `LEAST()` behavior against MySQL 8.4.9 for
  arity, `NULL`, integer, boolean, ASCII string, default collation, tied
  string identity, `DUAL`, `DO`, row labels, and table-backed projection.
- [x] Specify the narrow supported grammar, runtime domain classification,
  diagnostics, ownership boundaries, generated SQLite shape, and deferred
  surfaces.
- [x] Add parser/AST support for `GREATEST()` and `LEAST()` with
  MySQL-compatible argument-count diagnostics.
- [x] Extend scalar, `DO`, and row-scalar planning/execution for flat
  all-string/all-integer `GREATEST()` and `LEAST()` expressions.
- [x] Add MySQL-runtime expectation script and fast C parser/runtime tests.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review, commit, and push to `origin/main`.
