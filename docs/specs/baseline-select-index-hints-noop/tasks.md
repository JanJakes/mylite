# Baseline SELECT Index Hints No-Op Tasks

- [x] Review existing SELECT parser, AST, runtime planner, alias handling, and
  catalog index descriptor helpers.
- [x] Verify MySQL 8.4.9 behavior for the admitted index-hint subset.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Add parser/AST support for table index hints after SELECT table aliases.
- [x] Add runtime hint validation against MyLite-owned index descriptors.
- [x] Keep validated hints as physical SQLite planner no-ops.
- [x] Add focused parser/runtime tests and CMake registration if needed.
- [x] Update compatibility documentation with exact partial wording.
- [x] Run the MySQL expectation script, focused CTests, and full check workflow.
- [x] Run a review subagent, fix findings, commit, and push `origin/main`.
