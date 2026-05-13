# Baseline Inner Join SELECT Tasks

- [x] Research official MySQL 8.4 join and SELECT documentation.
- [x] Verify supported behavior and diagnostics against MySQL 8.4.9 runtime.
- [x] Specify the narrow two-source descriptor-backed join subset.
- [ ] Add MySQL expectation script for the supported join surface.
- [ ] Extend parser and AST support for two-source `JOIN` / `INNER JOIN` /
      `CROSS JOIN` with optional equality `ON`.
- [ ] Add descriptor source-scope planning, multi-source column resolution, and
      join-condition planning.
- [ ] Generate qualified physical SQLite SQL and bind existing predicate/limit
      values without materializing joins in MyLite.
- [ ] Add runtime tests for successful joins, diagnostics, persistence,
      temporary shadowing, and file-format invariants.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run focused parser/runtime tests, the MySQL expectation script, and the
      full check workflow.
- [ ] Review architecture, diagnostics, memory cleanup, and performance.
- [ ] Commit and push the completed feature.
