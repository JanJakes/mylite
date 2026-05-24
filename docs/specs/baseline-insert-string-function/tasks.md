# Baseline INSERT String Function Tasks

- [x] Review existing string slice, replacement, row-scalar planning, parser,
      and SQLite scalar-function registration code.
- [x] Verify `INSERT()` string-function behavior against MySQL 8.4.9 runtime
      for supported results, wrong arity syntax errors, positions, lengths,
      `NULL`, UTF-8 strings, labels, `DO`, and table-backed projections.
- [x] Write the independently authored feature specification.
- [x] Add parser/AST support for expression-context `INSERT()`.
- [x] Add MyLite string runtime substring-insert support and SQLite scalar
      registration.
- [x] Add scalar and row-scalar planner/runtime support.
- [x] Add MySQL expectation script and fast C runtime tests.
- [x] Update compatibility docs.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run full check workflow.
- [x] Review, commit, and push.
