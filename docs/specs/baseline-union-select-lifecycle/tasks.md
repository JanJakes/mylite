# Baseline Union Select Lifecycle Tasks

- [x] Read project architecture, query-expression compatibility docs, parser
  and runtime SELECT architecture, result builder APIs, and SQLite fork policy.
- [x] Research official MySQL 8.4 `UNION`, set-operation, and `SELECT`
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for duplicate handling, result labels,
  mixed `ALL`/distinct chains, column-count diagnostics, and ordering/limit
  clause boundaries.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, duplicate handling, unsupported surfaces,
  diagnostics, performance notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [ ] Update compatibility documentation for the exact limited subset.
- [ ] Extend AST/parser support for compound select statements without
  admitting unrelated set-operation grammar.
- [ ] Add runtime support for branch execution, column-count validation,
  metadata copying, `UNION ALL` append, and `UNION` duplicate removal.
- [ ] Add focused runtime C tests for success paths, diagnostics, result
  metadata, read-only persistence, independent handles, and unsupported forms.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused parser/runtime tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review final diff for MySQL behavior, result metadata, performance,
  cleanup, scope control, and compatibility accuracy.
- [ ] Commit and push the implementation slice.
