# Baseline Scalar Arithmetic Projection Tasks

- [x] Read current compatibility, scalar projection, parser, runtime, result,
  diagnostics, storage, and test context.
- [x] Research official MySQL 8.4 `SELECT`, expression, arithmetic operator,
  operator precedence, precision math, and `NULL` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for no-source and `DUAL` arithmetic
  projection, precedence, parentheses, aliases, `NULL`, booleans, scalar
  functions, row count, warnings, and overflow.
- [x] Write the independent feature spec with MyLite analyzer grammar snippets,
  ownership boundaries, arithmetic semantics, diagnostics, performance
  constraints, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [x] Commit and push the start-feature artifacts.
- [x] Admit no-source and `FROM DUAL` scalar arithmetic projection over `+`,
  binary `-`, and `*` without widening table-backed expression projection.
- [x] Preserve existing scalar value function operand restrictions unless the
  implementation deliberately and safely admits arithmetic there.
- [x] Add MyLite-owned checked signed-64 arithmetic evaluation with `NULL`
  propagation and deterministic overflow diagnostics.
- [x] Preserve warning-count and row-count ordering for mixed scalar projections.
- [x] Add runtime lifecycle tests for values, precedence, labels, aliases,
  warnings, row count, overflow, file safety, independent handles, and
  deterministic rejection of unsupported broader forms.
- [x] Update existing scalar projection tests that currently expect arithmetic
  rejection where this feature now admits it.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, overflow correctness, performance, cleanup,
  compatibility wording, and test relevance.
- [x] Commit, push `main`, and continue to the next baseline slice.
