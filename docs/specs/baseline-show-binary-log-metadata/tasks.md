# Baseline SHOW Binary Log Metadata Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for `SHOW BINARY LOG STATUS`, `SHOW BINARY LOGS`, and removed
  `SHOW MASTER STATUS`.
- [x] Specify the independently authored limited MyLite surface.
- [x] Add MySQL-runtime expectation artifact for column shape, diagnostics, and
  unsupported syntax diagnostics.
- [x] Add parser and AST support for `SHOW BINARY LOG STATUS` and
  `SHOW BINARY LOGS`.
- [x] Add runtime placeholder result building.
- [x] Add fast parser/runtime C tests.
- [x] Update `COMPATIBILITY.md` and detail compatibility docs.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for scope, architecture boundaries, metadata accuracy,
  and regression risk.
