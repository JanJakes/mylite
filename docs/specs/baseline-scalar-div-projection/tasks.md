# Baseline Scalar DIV Projection Tasks

- [x] Read current scalar arithmetic, unary arithmetic, and modulo specs,
  compatibility docs, parser/runtime code, and tests.
- [x] Review official MySQL 8.4 arithmetic, precedence, and `NULL` behavior
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `DIV`, signs, boolean operands,
  `NULL`, zero divisor warnings, labels, row count, warning count, precedence,
  boundary values, and accepted broader forms.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippet, runtime semantics, diagnostics, unsupported forms,
  performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [x] Commit and push the start-feature artifacts.
- [x] Add parser/AST support for infix `DIV` expression nodes.
- [x] Admit no-source and `FROM DUAL` scalar `DIV` projection without widening
  table-backed expression projection.
- [x] Add MyLite-owned signed-64 integer division with `NULL` propagation,
  zero-divisor warnings, and MySQL-compatible signed-minimum boundary errors.
- [x] Preserve in-statement diagnostics-count and row-count ordering for mixed
  scalar projections.
- [x] Add runtime/parser tests for values, precedence, labels, aliases, warning
  counts, diagnostics snapshot behavior, boundary values, file safety,
  independent handles, and deterministic rejection of unsupported forms.
- [x] Update existing scalar projection tests that currently expect `DIV`
  rejection where this feature now admits it.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`, or
  document that an existing test binary is reused.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, division-by-zero warning correctness,
  signed-64 arithmetic safety, performance, cleanup, compatibility wording, and
  test relevance.
