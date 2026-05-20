# Baseline ALTER TABLE COMMENT Tasks

- [x] Verify MySQL 8.4.9 behavior for syntax, metadata surfaces, diagnostics,
      algorithm/lock tails, temporary tables, and overlength comments.
- [x] Write the independently authored feature spec and grammar snippet.
- [x] Add parser/AST support for `ALTER TABLE ... COMMENT`.
- [x] Add durable catalog and temporary-catalog comment mutation helpers.
- [x] Implement descriptor-driven runtime planning/execution.
- [x] Add parser and runtime tests, including persistence and diagnostics.
- [x] Add a MySQL-runtime expectation script for the introduced behavior.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run the MySQL expectation script, focused CTests, build, and full check
      workflow.
- [x] Review the final diff, fix gaps, commit, and push.
