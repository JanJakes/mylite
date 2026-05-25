# Baseline Temporal Constructor Functions Tasks

- [x] Research official MySQL 8.4 date/time-function documentation.
- [x] Verify admitted behavior against MySQL 8.4.9 runtime.
- [x] Specify parser, ownership, runtime, diagnostics, and test scope.
- [x] Add MySQL expectation script for the verified subset.
- [x] Extend lexer/parser/AST support for `FROM_DAYS`, `MAKEDATE`, and `MAKETIME`.
- [x] Add MyLite-owned temporal constructor helpers and SQLite callbacks.
- [x] Wire source-free and row-scalar runtime evaluation.
- [x] Add fast C parser, lexer, and runtime tests.
- [x] Update compatibility documentation.
- [x] Run focused build/tests, MySQL expectation script, and `cmake --workflow --preset check`.
- [x] Commit, review, amend if needed, and push `main`.
