# Baseline Row Scalar Expressions Tasks

- [x] Create independently authored feature spec.
- [x] Verify `CONCAT()` and row-expression behavior against MySQL 8.4.9.
- [x] Add MySQL-runtime expectation artifact.
- [ ] Add parser/AST support for `CONCAT()` and zero-argument diagnostics.
- [ ] Add descriptor-backed row scalar SELECT planning and SQL lowering.
- [ ] Bind string/session/literal expression values instead of interpolating.
- [ ] Add fast runtime and parser tests.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run focused parser/runtime tests and MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, commit atomically, and push `main`.
