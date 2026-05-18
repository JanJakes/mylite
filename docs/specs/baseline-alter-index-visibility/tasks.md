# Baseline ALTER INDEX Visibility Tasks

- [x] Review existing parser, catalog, runtime, index metadata, and column
  visibility implementation.
- [x] Verify MySQL 8.4.9 behavior for `ALTER TABLE ... ALTER INDEX`
  visibility, metadata, diagnostics, option tails, and enforcement side
  effects.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Add parser/AST support for `ALTER TABLE ... ALTER INDEX ...`
  `VISIBLE|INVISIBLE`.
- [x] Add durable index descriptor visibility metadata and migration.
- [x] Add runtime planning/execution for catalog-only visibility changes.
- [x] Render index visibility in `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, and `SHOW CREATE TABLE`.
- [x] Add focused parser/runtime tests and CMake registration if needed.
- [x] Update compatibility documentation with exact partial wording.
- [x] Run the MySQL expectation script, focused CTests, and full check
  workflow.
- [x] Run a review subagent, fix findings, commit, and push `origin/main`.
