# Baseline JSON Introspection Functions Tasks

- [x] Review existing JSON type, JSON_VALID, JSON_EXTRACT, JSON_UNQUOTE,
  JSON_ARRAY, JSON_OBJECT, scalar projection, row-scalar projection, and SQLite
  function-registration code.
- [x] Verify MySQL 8.4.9 behavior for the admitted `JSON_TYPE()` and
  `JSON_LENGTH()` subset.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Add parser/AST support for `JSON_TYPE()` and `JSON_LENGTH()` plus
  argument-count diagnostics.
- [x] Add MyLite JSON runtime helpers for value type and shallow length with
  optional simple-path selection.
- [x] Register MyLite SQLite scalar callbacks for descriptor-backed row-scalar
  execution.
- [x] Implement scalar and row-scalar planning, SQL generation, and parameter
  binding for the supported subset.
- [x] Add focused runtime/parser tests and CMake registration.
- [x] Update compatibility documentation with exact partial wording.
- [x] Run the MySQL expectation script, focused CTests, and full check workflow.
- [x] Run a review subagent, fix findings, commit, and push `origin/main`.
