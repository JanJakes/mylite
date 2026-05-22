# Baseline SELECT Locking Clauses Tasks

- [x] Choose feature slug `baseline-select-locking-clauses`.
- [x] Read baseline project guidance, engineering standards, compatibility
      matrix, existing SELECT specs, parser/runtime SELECT paths, and SQLite
      fork policy.
- [x] Research official MySQL 8.4 SELECT locking-clause and InnoDB locking-read
      documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted no-op locking clauses,
      representative source `INSERT`/`REPLACE` selects, CTAS rejection,
      accepted-but-deferred options, and repeated/misplaced locking clauses.
- [x] Write independently authored feature spec with MyLite Lemon-syntax
      snippets and explicit ownership boundaries.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility docs for the exact supported subset.
- [x] Commit and push the start-feature artifacts.
- [x] Implement parser/AST support for admitted simple SELECT locking clauses.
- [x] Implement runtime no-op behavior and CTAS rejection for the admitted
      subset.
- [x] Add fast C parser/runtime tests and register any new test binary in
      `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
      no-op SQLite SQL generation, CTAS diagnostics, docs accuracy, and test
      relevance.
- [x] Commit, review with a subagent, amend if needed, push `main`, then
      continue to the next baseline slice.
