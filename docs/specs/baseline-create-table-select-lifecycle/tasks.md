# Baseline CREATE TABLE SELECT Lifecycle Tasks

- [x] Read project architecture, existing CREATE TABLE LIKE, INSERT SELECT,
  SELECT, catalog, parser, storage, compatibility docs, and SQLite policy.
- [x] Research official MySQL 8.4 `CREATE TABLE ... SELECT` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for supported syntax, result shape,
  diagnostics, source-first resolution, column inference, invisible columns,
  zero-row sources, and `IF NOT EXISTS`.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, generated SQLite shape, diagnostics, and
  test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Extend AST/parser support for `CREATE TABLE [IF NOT EXISTS] table [AS]
  SELECT ...` without admitting deferred CTAS forms.
- [ ] Add analyzer/planner support for source-first planning, target
  resolution, output descriptor inference, duplicate output names, and cleanup.
- [ ] Implement atomic catalog/physical table creation plus SQLite-side
  `INSERT INTO ... SELECT ...` row copying.
- [ ] Add focused runtime C tests for success paths, diagnostics, persistence,
  file format safety, independent handles, unsupported forms, and generation
  counters.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused parser/runtime tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review final diff for MySQL behavior, descriptor authority, performance,
  cleanup, scope control, and compatibility accuracy.
- [ ] Commit the implementation slice.
