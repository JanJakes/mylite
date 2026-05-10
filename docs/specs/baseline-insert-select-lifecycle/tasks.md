# Baseline Insert Select Lifecycle Tasks

- [x] Read project architecture, existing INSERT/SELECT specs, parser/runtime
  code, compatibility docs, and SQLite policy.
- [x] Research official MySQL 8.4 `INSERT` and `INSERT ... SELECT`
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for supported syntax, result shape,
  diagnostics, zero-row sources, omitted columns, invisible columns, and
  target/source resolution precedence.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, generated SQLite shape, validation policy,
  diagnostics, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend AST/parser support for `INSERT [INTO] table [(cols)] SELECT ...`
  without admitting deferred modifiers or unrelated query forms.
- [x] Add analyzer/planner support for target resolution, source `SELECT`
  planning, target/source column mapping, omitted target columns, descriptor
  defaults, and validation cleanup.
- [x] Implement streaming validation for selected `NULL`/integer values and
  SQLite-side physical `INSERT INTO ... SELECT ...`.
- [x] Add focused runtime C tests for success paths, diagnostics, persistence,
  file format safety, independent handles, and unsupported forms.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority,
  performance, cleanup, scope control, and compatibility accuracy.
- [x] Commit the implementation slice.
