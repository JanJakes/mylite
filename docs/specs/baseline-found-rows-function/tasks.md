# Baseline FOUND_ROWS Function Tasks

- [x] Choose feature slug `baseline-found-rows-function`.
- [x] Read baseline project guidance, engineering standards, compatibility
      matrix, current SELECT/function docs, and existing row-count/session
      state implementation.
- [x] Research official MySQL 8.4 `FOUND_ROWS()` and
      `SQL_CALC_FOUND_ROWS` documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for function result state,
      deprecation warnings, `SQL_CALC_FOUND_ROWS`, ordinary `SELECT` limit
      envelopes, non-`SELECT` preservation, and unsupported argument counts.
- [x] Write independently authored feature spec with MyLite Lemon-syntax
      snippets and explicit ownership boundaries.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility docs for the exact supported subset.
- [x] Commit and push the start-feature artifacts.
- [x] Implement parser/AST support for `FOUND_ROWS()` and the admitted
      `SQL_CALC_FOUND_ROWS` modifier forms.
- [x] Implement connection-local found-row state and scalar result generation.
- [x] Implement descriptor-driven pre-limit counting for the admitted
      `SQL_CALC_FOUND_ROWS` SELECT subset without full-row materialization.
- [x] Add fast C parser/runtime tests and register any new test binary in
      `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
      warning semantics, found-row state lifetime, performance, docs accuracy,
      and test relevance.
- [x] Commit, review with a subagent, amend if needed, push `main`, then
      continue to the next baseline slice.
- [x] Extend the documented MySQL 8.4.9 evidence and runtime coverage for
      `SQL_CALC_FOUND_ROWS` on one-row aggregate selects.
- [x] Extend descriptor `DISTINCT SQL_CALC_FOUND_ROWS` support to count
      distinct projected rows before `LIMIT`, including the joined descriptor
      source envelope.
