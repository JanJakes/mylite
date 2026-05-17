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
- [x] Implement descriptor validation for string grouped keys.
- [x] Generate collation-aware SQLite `GROUP BY` expressions for string grouped
  keys.
- [x] Add grouped result readback for string group keys.
- [x] Add C runtime tests for success, diagnostics, persistence, labels,
  ordering, limiting, and file-format safety.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, result semantics,
  descriptor authority, scope control, performance path, and compatibility
  wording.
- [x] Commit and push the implementation slice.
