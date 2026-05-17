# Baseline JSON_VALID Function Tasks

- [x] Review existing JSON type support, scalar function planning, row-scalar
  projection, row-scalar predicates, and SQLite function registration.
- [x] Verify MySQL 8.4.9 behavior for the admitted `JSON_VALID()` subset.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Add parser/AST support for `JSON_VALID(value)` and argument-count
  diagnostics.
- [x] Add a validation-only JSON runtime API separate from JSON storage
  canonicalization.
- [x] Register a MyLite SQLite scalar callback for descriptor-backed
  `JSON_VALID()` execution.
- [x] Implement scalar and row-scalar planning, SQL generation, and parameter
  binding for the supported subset.
- [x] Add focused runtime/parser tests and CMake registration.
- [x] Update compatibility documentation with exact partial wording.
- [x] Run the MySQL expectation script, focused CTests, and full check workflow.
- [x] Commit, run a review subagent, amend any findings, and push `origin/main`.
