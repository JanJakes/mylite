# Baseline GROUP BY Primary-Key Projection Tasks

- [x] Read prior grouped aggregate, joined aggregate, primary-key, catalog,
  storage, and compatibility specs.
- [x] Research official MySQL 8.4 `GROUP BY` and functional-dependence
  behavior.
- [x] Verify representative success and error behavior on MySQL 8.4.9.
- [x] Define the exact MyLite descriptor-driven subset and non-goals.
- [x] Add MySQL-runtime expectation script for this slice.
- [x] Extend grouped aggregate planning with descriptor projection columns.
- [x] Add primary-key coverage validation using catalog descriptors.
- [x] Add wildcard expansion validation for grouped projections.
- [x] Extend grouped `ORDER BY` validation for selected and unselected
  primary-key-dependent descriptor columns.
- [x] Generate SQLite SQL from validated descriptor projections.
- [x] Return projected descriptor values followed by aggregate values.
- [x] Add C runtime tests for success, diagnostics, persistence, and
  independent handles.
- [x] Update compatibility documentation.
- [x] Run focused grouped/query lifecycle tests, the MySQL expectation script,
  and `cmake --workflow --preset check`.
- [x] Review, commit, and push.
