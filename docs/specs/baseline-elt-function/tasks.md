# Baseline ELT Function Tasks

- [x] Verify `ELT()` behavior against MySQL 8.4.9 for admitted scalar literals,
  `NULL`, booleans, out-of-range selectors, warnings, and arity diagnostics.
- [x] Specify the narrow grammar, runtime ownership, diagnostics, and
  non-goals.
- [x] Add parser/AST support for `ELT()`.
- [x] Add runtime scalar evaluation for the admitted no-source, `DUAL`, and
  `DO` subset.
- [x] Add C runtime and parser tests.
- [x] Add MySQL 8.4.9 expectation script.
- [x] Update compatibility docs.
- [x] Run focused parser/runtime tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, and push.
