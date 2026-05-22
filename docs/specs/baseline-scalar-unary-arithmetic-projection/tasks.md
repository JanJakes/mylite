# Baseline Scalar Unary Arithmetic Projection Tasks

- [x] Read current compatibility, scalar projection, scalar arithmetic,
  parser, runtime, result, diagnostics, storage, and test context.
- [x] Research official MySQL 8.4 expression, arithmetic operator, operator
  precedence, and `NULL` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for no-source and `DUAL` unary
  arithmetic projection, precedence, repeated signs, aliases, `NULL`,
  booleans, scalar functions, row count, warnings, and overflow/deferred
  unsigned-width outcomes.
- [x] Write the independent feature spec with MyLite analyzer grammar snippets,
  ownership boundaries, unary arithmetic semantics, diagnostics, performance
  constraints, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [x] Commit and push the start-feature artifacts.
- [x] Admit no-source and `FROM DUAL` scalar unary arithmetic projection over
  unary `+` and unary `-` without widening table-backed expression projection.
- [x] Preserve existing scalar value function operand restrictions unless the
  implementation deliberately and safely admits unary arithmetic there.
- [x] Add MyLite-owned checked signed-64 unary arithmetic evaluation with
  `NULL` propagation and deterministic overflow diagnostics.
- [x] Preserve warning-count and row-count ordering for mixed scalar
  projections.
- [x] Add runtime lifecycle tests for values, precedence, labels, aliases,
  warnings, row count, overflow/deferred unsigned-width outcomes, file safety,
  independent handles, and deterministic rejection of unsupported broader
  forms.
- [x] Update existing scalar projection tests that currently expect unary
  arithmetic rejection where this feature now admits it.
- [x] Keep the existing scalar arithmetic test binary; no new CMake target was
  needed for this narrow expansion.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, unary overflow correctness, performance,
  cleanup, compatibility wording, and test relevance.
- [x] Commit, push `main`, and continue to the next baseline slice.
