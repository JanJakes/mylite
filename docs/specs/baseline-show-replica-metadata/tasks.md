# Baseline SHOW Replica Metadata Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for `SHOW REPLICA STATUS`, `SHOW REPLICAS`, removed `SHOW SLAVE`
  forms, and unsupported modifiers.
- [x] Specify the independently authored limited MyLite surface.
- [x] Add MySQL-runtime expectation artifact for column shape, zero-row
  behavior, diagnostics, and unsupported syntax diagnostics.
- [x] Add parser and AST support for `SHOW REPLICA STATUS` and `SHOW REPLICAS`.
- [x] Add runtime empty result-set building.
- [x] Add fast parser/runtime C tests.
- [x] Update `COMPATIBILITY.md` and detail compatibility docs.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for scope, architecture boundaries, metadata accuracy,
  and regression risk.
