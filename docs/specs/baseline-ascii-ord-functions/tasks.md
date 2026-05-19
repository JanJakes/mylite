# Baseline ASCII and ORD Functions Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
      behavior for `ASCII()` and `ORD()`.
- [x] Specify the supported grammar, runtime semantics, diagnostics, and
      ownership boundaries in `specs.md`.
- [x] Add MySQL-runtime expectation script for supported values, row-backed
      behavior, labels, and diagnostics.
- [x] Extend lexer/parser/AST support for `ASCII()` and `ORD()`.
- [x] Add MyLite-owned string codepoint evaluator and private SQLite scalar
      helpers.
- [x] Add scalar, `DO`, row-scalar, persistence, and diagnostic C tests.
- [x] Update compatibility documentation.
- [x] Run MySQL expectation script, focused CTests, full build, and check
      workflow.
- [x] Review the final diff, commit, and push.
