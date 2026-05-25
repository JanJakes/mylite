# Baseline INTERVAL Function Tasks

- [x] Verify `INTERVAL()` behavior against MySQL 8.4.9 for syntax errors,
  integer and boolean values, `NULL` search, duplicate thresholds, signed
  boundaries, `DUAL`, `DO`, row labels, table-backed projection, and deferred
  coercions.
- [x] Specify the narrow supported grammar, runtime threshold validation,
  diagnostics, ownership boundaries, generated SQLite shape, and deferred
  surfaces.
- [x] Add parser/AST support for `INTERVAL()` while preserving MySQL-compatible
  syntax errors for zero- and one-argument forms.
- [x] Extend scalar, `DO`, and row-scalar planning/execution for sorted
  integer-domain `INTERVAL()` expressions.
- [x] Add MySQL-runtime expectation script and fast C parser/runtime tests.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review, commit, and push to `origin/main`.
