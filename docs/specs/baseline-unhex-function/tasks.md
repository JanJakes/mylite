# Baseline UNHEX Function Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
      behavior for `UNHEX()`.
- [x] Specify the supported grammar, runtime semantics, diagnostics, and
      ownership boundaries in `specs.md`.
- [x] Add MySQL-runtime expectation script for supported values, warnings,
      arity errors, and table-backed behavior.
- [x] Extend lexer/parser/AST support for `UNHEX()` and wrong-arity nodes.
- [x] Add MyLite-owned byte decoder and private SQLite scalar helper.
- [x] Make no-source scalar projection byte-safe for binary function results.
- [x] Add scalar, `DO`, row-scalar, warning, and diagnostic C tests.
- [x] Update compatibility documentation.
- [x] Run MySQL expectation script, focused CTests, full build, and check
      workflow.
- [x] Review the final diff, commit, and push.
