# Baseline SHOW STATUS Tasks

- [x] Research official MySQL 8.4 `SHOW STATUS` and status-variable docs.
- [x] Verify MySQL 8.4.9 runtime behavior for result labels, scope, `LIKE`,
      unsupported clauses, diagnostics, and status-result row count behavior.
- [x] Define the deliberately limited MyLite status registry and documented
      placeholder values.
- [x] Add parser/AST support for `SHOW [GLOBAL|SESSION|LOCAL] STATUS [LIKE
      'pattern']`.
- [x] Add runtime result construction from the MyLite-owned status registry.
- [x] Add MySQL 8.4.9 expectation script for the admitted and rejected surface.
- [x] Add fast C parser/runtime tests, including file-backed and independent
      handle coverage.
- [x] Update `COMPATIBILITY.md`, `docs/compatibility/sql-show-statements.md`,
      and `docs/compatibility/runtime-status-variables.md`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review architecture boundaries, scope control, docs accuracy, and test
      relevance before committing.
