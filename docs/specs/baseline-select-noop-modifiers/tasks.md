# Baseline Select No-op Modifiers Tasks

- [x] Choose feature slug `baseline-select-noop-modifiers`.
- [x] Read baseline project guidance, engineering standards, compatibility
      matrix, existing SELECT modifier specs, and parser/runtime SELECT paths.
- [x] Research official MySQL 8.4 SELECT modifier documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted no-op modifiers,
      `SQL_NO_CACHE` deprecation warnings, warning order with
      `FOUND_ROWS()` / `SQL_CALC_FOUND_ROWS`, and representative unsupported
      syntax.
- [x] Write independently authored feature spec with MyLite Lemon-syntax
      snippets and explicit ownership boundaries.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility docs for the exact supported subset.
- [x] Commit and push the start-feature artifacts.
- [x] Implement parser/AST support for canonical no-op SELECT modifiers.
- [x] Implement planner/runtime warning behavior for `SQL_NO_CACHE`.
- [x] Add fast C parser/runtime tests and register any new test binary in
      `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
      warning order, no-op SQLite SQL generation, docs accuracy, and test
      relevance.
- [x] Commit, review with a subagent, amend if needed, push `main`, then
      continue to the next baseline slice.
