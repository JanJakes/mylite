# Baseline SET Connection Character Set Tasks

Add the next narrow connection-bootstrap slice for fixed `SET NAMES` and
`SET CHARACTER SET` forms.

## Checklist

- [x] Verify MySQL 8.4.9 behavior for supported forms, quoted names,
  diagnostics, result shape, warning count, and `ROW_COUNT()`.
- [x] Specify syntax, runtime semantics, diagnostics, architecture boundaries,
  SQLite non-involvement, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Add parser token mapping, grammar, AST kinds, AST names, and parser
  helpers for fixed `SET NAMES` / `SET CHARACTER SET` statements.
- [x] Add runtime execution that validates fixed `utf8mb4` /
  `utf8mb4_0900_ai_ci` names and returns an empty non-row result.
- [x] Add C parser and runtime tests, including diagnostics, result metadata,
  preamble preservation, reopen behavior, and independent handles.
- [x] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, fixed-session
  semantics, diagnostics, docs accuracy, cleanup, and test relevance.
