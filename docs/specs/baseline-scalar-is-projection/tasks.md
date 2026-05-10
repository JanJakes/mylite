# Baseline Scalar IS Projection Tasks

- [x] Read current scalar expression, arithmetic, modulo, `DIV`, comparison,
  logical, and descriptor-backed `IS` predicate specs, compatibility docs,
  parser/runtime code, and tests.
- [x] Review official MySQL 8.4 comparison-operator, expression, and
  precedence documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for scalar `IS` truth tables, nonzero
  truthiness, `NULL`, child warning behavior, logical child short-circuiting,
  precedence, labels, row count, warning count, boundary operands, syntax
  errors, direct chaining, and accepted broader forms.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippet, runtime semantics, diagnostics, unsupported forms,
  performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Add parser support for scalar `IS` expressions without changing
  descriptor-backed predicate planning.
- [ ] Admit no-source and `FROM DUAL` scalar `IS` projection without widening
  table-backed expression projection.
- [ ] Add MyLite-owned scalar `IS` evaluation with MySQL 8.4.9 truth semantics.
- [ ] Preserve evaluated child arithmetic warnings, in-statement diagnostics
  count behavior, row-count ordering, and logical child short-circuiting.
- [ ] Add runtime/parser tests for truth tests, precedence, labels, aliases,
  warning counts, diagnostics snapshot behavior, boundary operands, file
  safety, independent handles, and deterministic rejection of unsupported
  forms.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`, or
  document that an existing test binary is reused.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, scalar `IS` truth correctness, child warning
  behavior, signed-64 conversion safety, performance, cleanup, compatibility
  wording, and test relevance.
