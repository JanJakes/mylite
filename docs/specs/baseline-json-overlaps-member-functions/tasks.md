# Baseline JSON_OVERLAPS and MEMBER OF Tasks

- [x] Read project architecture, engineering standards, compatibility docs,
  existing JSON search specs/tests, parser/AST sources, JSON runtime sources,
  row-scalar planner, SQLite callback registration, and SQLite fork policy.
- [x] Research official MySQL 8.4 JSON search documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for scalar/array/object overlap,
  nested equality, `MEMBER OF()` scalar coercion, nonarray right operands,
  `NULL`, labels, `DO`, diagnostics, and wrong arity/syntax.
- [x] Write the independent feature spec with MyLite Lemon-style grammar
  snippets, ownership boundaries, scalar evaluation semantics, diagnostics,
  performance constraints, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend lexer/parser/AST support for `JSON_OVERLAPS()` and
  `MEMBER OF()`.
- [x] Add JSON DOM equality, overlap, and member-of helpers.
- [x] Add scalar/no-source execution and private SQLite row-scalar callbacks.
- [x] Add row-scalar planning, SQL generation, and parameter binding support.
- [x] Add parser, runtime, metadata, persistence, and diagnostic tests.
- [x] Register any new test binary in `packages/libmylite` CMake files.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, result metadata, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit, push `main`, and continue to the next baseline slice.
