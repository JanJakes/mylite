# Baseline UUID Conversion Functions Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
      behavior for `IS_UUID()`, `UUID_TO_BIN()`, and `BIN_TO_UUID()`.
- [x] Specify the supported grammar, runtime semantics, diagnostics, SQLite
      integration, and ownership boundaries in `specs.md`.
- [x] Add MySQL-runtime expectation script for supported values, errors,
      warnings, arity, and table-backed behavior.
- [ ] Extend lexer/parser/AST support for UUID conversion functions and
      wrong-arity nodes.
- [ ] Add MyLite-owned UUID parser/formatter/swap helpers and private SQLite
      scalar helpers.
- [ ] Add no-source, `DO`, row-scalar, binary-result, warning, and diagnostic
      C tests.
- [ ] Update compatibility documentation.
- [ ] Run MySQL expectation script, focused CTests, full build, and check
      workflow.
- [ ] Review the final diff, commit, and push.
