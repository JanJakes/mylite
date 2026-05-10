# Baseline Scalar Modulo Projection Tasks

- [x] Read baseline scalar arithmetic and unary arithmetic specs, compatibility
  docs, parser/runtime code, and tests.
- [x] Review official MySQL 8.4 arithmetic, mathematical-function, precedence,
  and `NULL` behavior documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `%`, infix `MOD`, signs, boolean
  operands, `NULL`, modulo by zero warnings, labels, row count, warning count,
  precedence, boundary values, and accepted broader forms.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippets, runtime semantics, diagnostics, unsupported forms,
  performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Add parser/AST support for `%` token mapping, infix `MOD` expression
  nodes, and two-argument `MOD()` function syntax without admitting
  wrong-arity `MOD()` forms.
- [ ] Admit no-source and `FROM DUAL` scalar modulo projection without widening
  table-backed expression projection.
- [ ] Add MyLite-owned signed-64 modulo evaluation with `NULL` propagation,
  safe `INT64_MIN % -1`, and staged division-by-zero warnings.
- [ ] Preserve in-statement diagnostics-count and row-count ordering for mixed
  scalar projections.
- [ ] Add runtime/parser tests for values, precedence, labels, aliases, warning
  counts, diagnostics snapshot behavior, boundary values, file safety,
  independent handles, and deterministic rejection of unsupported forms.
- [ ] Update existing scalar projection tests that currently expect `%` or
  infix `MOD` rejection where this feature now admits them.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`, or
  document that an existing test binary is reused.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, modulo-by-zero warning correctness,
  signed-64 arithmetic safety, performance, cleanup, compatibility wording, and
  test relevance.
