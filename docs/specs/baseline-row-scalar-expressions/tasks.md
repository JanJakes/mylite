# Baseline Row Scalar Expressions Tasks

- [x] Create independently authored feature spec.
- [x] Verify `CONCAT()` and row-expression behavior against MySQL 8.4.9.
- [x] Add MySQL-runtime expectation artifact.
- [x] Add parser/AST support for `CONCAT()` and zero-argument diagnostics.
- [x] Add descriptor-backed row scalar SELECT planning and SQL lowering.
- [x] Bind string/session/literal expression values instead of interpolating.
- [x] Add fast runtime and parser tests.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused parser/runtime tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, commit atomically, and push `main`.
