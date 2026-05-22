# Baseline Scalar Bitwise Projection Tasks

- [x] Read baseline docs, engineering standards, compatibility matrix, existing
  scalar arithmetic/modulo/DIV/logical specs, parser/runtime code, and SQLite
  fork policy.
- [x] Research official MySQL 8.4 bitwise operator documentation and operator
  precedence.
- [x] Verify MySQL 8.4.9 runtime behavior for supported values, precedence,
  `NULL` propagation, shifts, boundaries, warnings, errors, and deferred forms.
- [x] Write the independently authored feature spec.
- [x] Add MySQL-runtime expectation artifact for the feature.
- [x] Implement parser/AST support for bitwise operator tokens and precedence.
- [x] Implement MyLite-owned scalar bitwise runtime evaluation.
- [x] Add parser and runtime tests under `packages/libmylite/tests/`.
- [x] Update compatibility docs for the exact limited subset.
- [x] Run MySQL expectations, focused CTest entries, `cmake --build --preset dev`,
  and `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, unsigned formatting, expression
  scope, diagnostics, architecture boundaries, file safety, and test coverage.
- [x] Commit, review with a subagent, amend if needed, push `main`, then
  continue to the next baseline slice.
