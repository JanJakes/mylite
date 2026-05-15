# Baseline REGEXP / RLIKE Predicates Tasks

- [x] Audit existing lexer, parser, AST, predicate planning, SQLite function
  registration, and `LIKE` predicate support.
- [x] Verify MySQL 8.4.9 behavior for the admitted `REGEXP` / `RLIKE`
  predicate subset.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Extend parser and AST support for descriptor `REGEXP` / `RLIKE`
  predicates.
- [x] Add a MyLite-owned ASCII baseline regex matcher and SQLite scalar
  function registration.
- [x] Implement descriptor-driven `REGEXP` / `RLIKE` planning and SQLite
  predicate generation without MyLite row materialization.
- [x] Extend runtime tests for success, diagnostics, DML effects, persistence,
  and preamble safety.
- [x] Update compatibility documentation with limited wording.
- [x] Run the MySQL expectation script, focused CTests, and full check workflow.
- [x] Commit, push to `origin/main`, and run a review subagent.
