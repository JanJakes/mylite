# Baseline SUM Aggregate Tasks

- [x] Read project architecture, compatibility, parser, runtime aggregate,
  diagnostics, and storage context.
- [x] Research official MySQL 8.4 aggregate and function-name parsing
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `SUM(column)`, `NULL` handling,
  empty and no-match inputs, labels, predicates, accepted-but-deferred forms,
  function-name whitespace, result overflow beyond signed 64 bits, and
  diagnostics.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, runtime semantics, physical SQLite handling,
  diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST node name, keyword mapping, and parser tests.
- [ ] Implement descriptor-driven `SUM(column)` planning and execution while
  keeping SQLite responsible for scans and in-range aggregation.
- [ ] Add C runtime tests for success, diagnostics, persistence, and overflow.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, result semantics,
  descriptor authority, scope control, and compatibility wording.
- [ ] Commit the implementation slice.
