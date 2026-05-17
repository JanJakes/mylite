# Baseline GROUP BY String Column Tasks

- [x] Read project architecture, compatibility, grouped aggregate, `HAVING`,
  string storage, string ordering, result, storage, and SQLite bootstrap
  context.
- [x] Research official MySQL 8.4 `SELECT`, `GROUP BY`, aggregate, charset, and
  collation documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `CHAR`, `VARCHAR`, and `TEXT`
  grouping, `NULL` keys, ASCII case folding, `HAVING`, ordering, limiting,
  warning count, and row-count state.
- [x] Write the independent feature spec with ownership boundaries, grammar
  snippet, runtime semantics, generated SQL shape, diagnostics, and test plan.
- [x] Add a MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Implement descriptor validation for string grouped keys.
- [ ] Generate collation-aware SQLite `GROUP BY` expressions for string grouped
  keys.
- [ ] Add grouped result readback for string group keys.
- [ ] Add C runtime tests for success, diagnostics, persistence, labels,
  ordering, limiting, and file-format safety.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, result semantics,
  descriptor authority, scope control, performance path, and compatibility
  wording.
- [ ] Commit and push the implementation slice.
