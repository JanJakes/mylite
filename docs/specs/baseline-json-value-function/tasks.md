# Baseline JSON_VALUE Function Tasks

- [x] Review existing JSON extraction, introspection, parser, row-scalar
      planning, and SQLite scalar-function registration code.
- [x] Verify `JSON_VALUE()` behavior against MySQL 8.4.9 runtime for supported
      results, syntax errors, invalid JSON warnings, invalid paths, scalar
      inputs, labels, `DO`, table-backed projections, and result-column
      metadata.
- [x] Write the independently authored feature specification.
- [x] Add parser/AST token and grammar support for `JSON_VALUE()`.
- [x] Add MyLite JSON runtime value extraction.
- [x] Add scalar and row-scalar planner/runtime support.
- [x] Add MySQL expectation script and fast C runtime tests.
- [x] Update compatibility docs.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run full check workflow.
- [x] Review, commit, and push.
