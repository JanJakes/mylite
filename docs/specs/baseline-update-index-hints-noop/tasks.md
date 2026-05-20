# Baseline UPDATE Index Hints No-Op Tasks

- [x] Review existing `UPDATE` parser/runtime and reusable `SELECT` index-hint
  validation.
- [x] Verify MySQL 8.4.9 behavior for single-table `UPDATE` index hints.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Add parser/AST support for update target index hints.
- [x] Add runtime hint validation against MyLite-owned index descriptors.
- [x] Keep validated hints as physical SQLite planner no-ops.
- [x] Add focused parser/runtime tests and CMake registration if needed.
- [x] Update compatibility documentation with exact partial wording.
- [x] Run the MySQL expectation script, focused CTests, and full check workflow.
- [x] Run a review subagent, fix findings, commit, and push `origin/main`.
