# Baseline JSON_REPLACE Function Tasks

- [x] Verify `JSON_REPLACE()` behavior against MySQL 8.4.9 for admitted scalar
  literals, JSON values, missing-path no-ops, `NULL`, warnings, and diagnostics.
- [x] Specify the narrow grammar, runtime ownership, diagnostics, non-goals, and
  JSON mutation semantics.
- [x] Add parser/AST support for `JSON_REPLACE()`.
- [x] Add runtime JSON replacement support for admitted scalar and row-scalar
  contexts.
- [x] Add C runtime and parser tests.
- [x] Add MySQL 8.4.9 expectation script.
- [x] Update compatibility docs.
- [x] Run focused parser/runtime tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, and push.
