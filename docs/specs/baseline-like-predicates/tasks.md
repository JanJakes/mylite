# Baseline LIKE Predicates Tasks

- [x] Audit existing string equality, predicate, SQL-mode, and descriptor DML
  support.
- [x] Verify MySQL 8.4.9 behavior for the admitted `LIKE` / `NOT LIKE`
  predicate subset.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [ ] Extend parser and AST support for descriptor `LIKE` predicates.
- [ ] Implement descriptor-driven `LIKE` / `NOT LIKE` planning and SQLite
  predicate generation without MyLite row materialization.
- [ ] Extend runtime tests for success, diagnostics, DML effects, SQL mode,
  persistence, and preamble safety.
- [ ] Update compatibility documentation with limited wording.
- [ ] Run the MySQL expectation script, focused CTests, and full check workflow.
- [ ] Commit, push to `origin/main`, and run a review subagent.
