# Baseline SET TRANSACTION Tasks

- [x] Research official MySQL 8.4 `SET TRANSACTION` syntax and scope rules.
- [x] Verify MySQL 8.4.9 runtime behavior for success forms, active
  transaction rejection, read-only persistent writes, next-characteristic
  consumption, session defaults, and temporary-table DML.
- [x] Specify the MyLite grammar, ownership boundaries, runtime semantics,
  diagnostics, non-goals, and test plan.
- [x] Add MySQL expectation script for this feature.
- [x] Add parser and AST support for the supported grammar subset.
- [x] Add connection-local transaction characteristic state.
- [x] Add runtime handling for `SET TRANSACTION` and `SET SESSION TRANSACTION`.
- [x] Enforce read-only access mode for persistent-table DML while allowing
  temporary-table DML.
- [x] Add runtime C tests and parser tests.
- [x] Update `COMPATIBILITY.md` and SQL transaction compatibility docs.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `main`.
