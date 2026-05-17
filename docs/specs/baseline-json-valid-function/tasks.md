# Baseline JSON_VALID Function Tasks

- [x] Review existing JSON type support, scalar function planning, row-scalar
  projection, row-scalar predicates, and SQLite function registration.
- [x] Verify MySQL 8.4.9 behavior for the admitted `JSON_VALID()` subset.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [ ] Add parser/AST support for `JSON_VALID(value)` and argument-count
  diagnostics.
- [ ] Add a validation-only JSON runtime API separate from JSON storage
  canonicalization.
- [ ] Register a MyLite SQLite scalar callback for descriptor-backed
  `JSON_VALID()` execution.
- [ ] Implement scalar and row-scalar planning, SQL generation, and parameter
  binding for the supported subset.
- [ ] Add focused runtime/parser tests and CMake registration.
- [ ] Update compatibility documentation with exact partial wording.
- [ ] Run the MySQL expectation script, focused CTests, and full check workflow.
- [ ] Commit, run a review subagent, amend any findings, and push `origin/main`.
