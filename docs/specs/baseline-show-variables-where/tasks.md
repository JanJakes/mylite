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
- [x] Update compatibility docs for the exact supported subset.
- [x] Implement parser/AST support for `SHOW VARIABLES WHERE`.
- [x] Implement runtime predicate evaluation over the MyLite system-variable
      registry without SQLite SQL generation.
- [x] Add fixed GTID variable rows and scalar reads with MySQL-compatible scope
      diagnostics for the supported subset.
- [x] Add fast C parser/runtime tests and register any new test binary in
      `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
      diagnostics, performance, docs accuracy, and test relevance.
- [x] Commit, push `main`, review with a subagent, amend if needed, then
      continue to the next baseline slice.
