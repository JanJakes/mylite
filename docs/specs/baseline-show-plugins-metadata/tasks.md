# Baseline SHOW PLUGINS Metadata Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for `SHOW PLUGINS` and `INFORMATION_SCHEMA.PLUGINS`.
- [x] Specify the independently authored limited MyLite surface.
- [x] Add MySQL-runtime expectation artifact for result shapes, metadata, and
  unsupported syntax diagnostics.
- [x] Add parser and AST support for `SHOW PLUGINS`.
- [x] Add runtime `SHOW PLUGINS` result building.
- [x] Register synthetic `INFORMATION_SCHEMA.PLUGINS` metadata and row.
- [x] Add fast parser/runtime C tests.
- [x] Update `COMPATIBILITY.md` and detail compatibility docs.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for scope, architecture boundaries, metadata accuracy,
  and regression risk.
