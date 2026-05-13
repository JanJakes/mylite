# Baseline utf8mb4 Legacy Collations Tasks

- [x] Verify MySQL 8.4.9 behavior for admitted legacy `utf8mb4` collations,
  `SHOW CREATE TABLE`, `CREATE TABLE LIKE`, `ALTER TABLE DEFAULT COLLATE`,
  `SET NAMES ... COLLATE`, `SHOW COLLATION`, and
  `INFORMATION_SCHEMA.COLLATIONS`.
- [x] Write the independently authored feature spec.
- [x] Add the MySQL 8.4.9 expectation script.
- [ ] Add table descriptor charset/collation catalog fields and migration.
- [ ] Preserve table collation through `CREATE TABLE`, `CREATE TABLE ... LIKE`,
  `SHOW CREATE TABLE`, `ALTER TABLE`, reopen, and `INFORMATION_SCHEMA.TABLES`.
- [ ] Extend admitted static collation rows and `SET NAMES ... COLLATE`
  session readback.
- [ ] Extend runtime tests for success, metadata, diagnostics, persistence,
  independent handles, and preamble safety.
- [ ] Update compatibility documentation with limited wording.
- [ ] Run the MySQL expectation script, focused CTests, and full check workflow.
- [ ] Commit, push to `origin/main`, and run a review subagent.
