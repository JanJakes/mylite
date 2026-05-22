# Baseline Multi-Action ALTER Defaults Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
      behavior for comma-separated `ALTER COLUMN SET/DROP DEFAULT` actions.
- [x] Specify supported grammar, runtime behavior, diagnostics, catalog
      ownership, SQLite integration, and compatibility limits.
- [x] Add MySQL-runtime expectation script for successful metadata changes,
      rollback, same-statement added-column diagnostics, and mixed actions.
- [x] Extend MyLite parser grammar and parser tests.
- [x] Split single-action default catalog writes into in-mutation helpers and
      wire them into multi-action execution.
- [x] Add runtime tests for successful default action lists, rollback,
      diagnostics, persistence, preamble preservation, and result shape.
- [x] Update compatibility documentation.
- [x] Run focused build/tests, MySQL expectation script, and full check
      workflow.
- [x] Review and amend release-gate findings.
- [x] Commit and push to `origin/main`.
