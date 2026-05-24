# Baseline JSON_KEYS Function Tasks

- [x] Review existing JSON type, extraction, introspection, contains, parser,
      row-scalar planning, and SQLite scalar-function registration code.
- [x] Verify `JSON_KEYS()` behavior against MySQL 8.4.9 runtime for supported
      results, arity errors, invalid JSON, invalid paths, binary input, scalar
      input, wildcard paths, labels, `DO`, and table-backed projections.
- [x] Write the independently authored feature specification.
- [x] Add parser/AST token and grammar support for `JSON_KEYS()`.
- [x] Add MyLite JSON runtime key-array extraction.
- [x] Add scalar and row-scalar planner/runtime support.
- [x] Add MySQL expectation script and fast C runtime tests.
- [x] Update compatibility docs.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run full check workflow.
- [x] Review, commit, and push.
