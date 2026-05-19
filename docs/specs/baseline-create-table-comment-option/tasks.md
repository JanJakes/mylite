# Baseline CREATE TABLE COMMENT Option Tasks

- [x] Verify MySQL 8.4.9 behavior for table comments, duplicates, escaping,
      metadata surfaces, `CREATE TABLE ... LIKE`, temporary tables, and
      overlength diagnostics.
- [x] Write the independently authored feature spec and grammar snippet.
- [x] Add parser/AST support for table-level `COMMENT` options.
- [x] Add durable and temporary table descriptor comment storage.
- [x] Render descriptor comments in `SHOW CREATE TABLE`.
- [x] Report descriptor comments in `SHOW TABLE STATUS` and
      `INFORMATION_SCHEMA.TABLES`.
- [x] Add parser and runtime tests, including persistence and diagnostics.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run the MySQL expectation script, focused CTests, build, and full check
      workflow.
- [x] Review the final diff, fix gaps, commit, and push.
