# Baseline Inner Join SELECT Tasks

- [x] Research official MySQL 8.4 join and SELECT documentation.
- [x] Verify supported behavior and diagnostics against MySQL 8.4.9 runtime.
- [x] Specify the narrow two-source descriptor-backed join subset.
- [x] Add MySQL expectation script for the supported join surface.
- [x] Extend parser and AST support for two-source `JOIN` / `INNER JOIN` /
      `CROSS JOIN` with optional equality `ON`.
- [x] Add descriptor source-scope planning, multi-source column resolution, and
      join-condition planning.
- [x] Generate qualified physical SQLite SQL and bind existing predicate/limit
      values without materializing joins in MyLite.
- [x] Add runtime tests for successful joins, diagnostics, persistence,
      temporary shadowing, and file-format invariants.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused parser/runtime tests, the MySQL expectation script, and the
      full check workflow.
- [x] Review architecture, diagnostics, memory cleanup, and performance.
- [x] Commit and push the completed feature.
