# Baseline Scalar Comparison Projection Tasks

- [x] Read current scalar expression, arithmetic, modulo, and `DIV` specs,
  compatibility docs, parser/runtime code, and tests.
- [x] Review official MySQL 8.4 comparison, expression, precedence, and `NULL`
  behavior documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for scalar comparisons, `NULL`, `<=>`,
  booleans, arithmetic operands, warning-producing child expressions,
  precedence, labels, row count, warning count, boundary values, syntax errors,
  and accepted broader forms.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippet, runtime semantics, diagnostics, unsupported forms,
  performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Add parser support for comparison binary expressions in scalar expression
  grammar without changing descriptor-backed predicate planning.
- [ ] Admit no-source and `FROM DUAL` scalar comparison projection without
  widening table-backed expression projection.
- [ ] Add MyLite-owned signed-64 comparison evaluation with ordinary `NULL`
  propagation and `<=>` NULL-safe equality semantics.
- [ ] Preserve child arithmetic warnings, in-statement diagnostics-count, and
  row-count ordering for mixed scalar projections.
- [ ] Add runtime/parser tests for values, precedence, labels, aliases, warning
  counts, diagnostics snapshot behavior, boundary values, file safety,
  independent handles, and deterministic rejection of unsupported forms.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`, or
  document that an existing test binary is reused.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, `NULL` and `<=>` correctness, signed-64
  conversion safety, performance, cleanup, compatibility wording, and test
  relevance.
