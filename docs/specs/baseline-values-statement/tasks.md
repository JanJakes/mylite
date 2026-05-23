# Baseline VALUES Statement Tasks

- [x] Read project architecture, query-expression compatibility docs, parser
  and runtime result-set architecture, result builder APIs, and SQLite fork
  policy.
- [x] Research official MySQL 8.4 `VALUES` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for result labels, row count state,
  row-constructor validation, limits, order-designator validation, and
  diagnostics.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, unsupported surfaces, diagnostics,
  performance notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend AST/parser support for standalone `VALUES` without admitting
  unrelated query-expression grammar.
- [x] Add parser tests for accepted and rejected `VALUES` forms.
- [x] Add runtime support for literal row conversion, result construction,
  order-designator validation, limit slicing, and diagnostics.
- [x] Add focused runtime C tests for success paths, diagnostics, result
  metadata, no-schema execution, cleanup, and unsupported forms.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, result metadata, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
