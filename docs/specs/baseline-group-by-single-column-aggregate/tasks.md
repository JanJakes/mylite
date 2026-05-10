# Baseline GROUP BY Single-Column Aggregate Tasks

- [x] Read project architecture, compatibility, parser, runtime aggregate,
  diagnostics, result, storage, and SQLite bootstrap context.
- [x] Research official MySQL 8.4 `SELECT`, aggregate, and `GROUP BY`
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for grouped aggregates, `NULL`
  grouping, ordering, limiting, labels, row-count state, and diagnostics.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, runtime semantics, generated SQL shape, diagnostics,
  and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST node name, keyword mapping, and parser tests.
- [ ] Implement descriptor-driven grouped aggregate planning and execution
  while keeping SQLite responsible for scans, filtering, grouping, ordering,
  and limiting.
- [ ] Add C runtime tests for success, diagnostics, persistence, labels,
  ordering, limiting, and file-format safety.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, result semantics,
  descriptor authority, scope control, performance path, and compatibility
  wording.
- [ ] Commit the implementation slice.
