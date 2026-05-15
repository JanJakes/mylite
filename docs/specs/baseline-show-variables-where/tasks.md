# Baseline SHOW VARIABLES WHERE Tasks

- [x] Choose feature slug `baseline-show-variables-where`.
- [x] Read baseline project guidance, engineering standards, compatibility
      matrix, existing SHOW VARIABLES docs, and current system-variable docs.
- [x] Research official MySQL 8.4 `SHOW VARIABLES` and system-variable docs.
- [x] Verify MySQL 8.4.9 runtime behavior for `WHERE` filters, output-column
      resolution, GTID variables, warnings, row-count behavior, and unsupported
      clauses.
- [x] Write independently authored feature spec with MyLite Lemon-syntax
      snippets and explicit ownership boundaries.
- [x] Add MySQL-runtime expectation script for this feature.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Implement parser/AST support for `SHOW VARIABLES WHERE`.
- [ ] Implement runtime predicate evaluation over the MyLite system-variable
      registry without SQLite SQL generation.
- [ ] Add fixed GTID variable rows and scalar reads with MySQL-compatible scope
      diagnostics for the supported subset.
- [ ] Add fast C parser/runtime tests and register any new test binary in
      `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused parser/runtime tests and the MySQL expectation script.
- [ ] Run `cmake --build --preset dev`.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, MySQL evidence,
      diagnostics, performance, docs accuracy, and test relevance.
- [ ] Commit, push `main`, review with a subagent, amend if needed, then
      continue to the next baseline slice.
