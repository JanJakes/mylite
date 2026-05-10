# Baseline CASE Operator Tasks

- [x] Read current scalar expression, arithmetic, comparison, logical, scalar
  `IS`, and control-flow specs, compatibility docs, parser/runtime code, and
  tests.
- [x] Review official MySQL 8.4 flow-control, expression, and precedence
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for searched `CASE`, simple `CASE`,
  no-match `NULL`, branch ordering, warnings, diagnostics, labels, nested
  forms, accepted broader forms, and syntax errors.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippet, runtime semantics, diagnostics, unsupported forms,
  performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Add parser/AST support for searched and simple scalar `CASE`
  expressions.
- [ ] Admit no-source and `FROM DUAL` scalar `CASE` projection without
  widening table-backed expression projection.
- [ ] Add MyLite-owned scalar `CASE` evaluation with MySQL 8.4.9 branch
  selection and `NULL` semantics.
- [ ] Preserve evaluated child arithmetic warnings, skipped-branch warning
  suppression, diagnostics count behavior, row-count ordering, and scalar
  short-circuiting.
- [ ] Add runtime/parser tests for searched/simple truth behavior, precedence,
  labels, aliases, warning counts, diagnostics snapshot behavior, boundary
  operands, file safety, independent handles, and deterministic rejection of
  unsupported forms.
- [ ] Register the new runtime test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, `CASE` branch correctness, child warning
  behavior, signed-64 conversion safety, performance, cleanup, compatibility
  wording, and test relevance.
