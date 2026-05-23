# Baseline GROUP BY Primary-Key Projection Tasks

- [x] Read prior grouped aggregate, joined aggregate, primary-key, catalog,
  storage, and compatibility specs.
- [x] Research official MySQL 8.4 `GROUP BY` and functional-dependence
  behavior.
- [x] Verify representative success and error behavior on MySQL 8.4.9.
- [x] Define the exact MyLite descriptor-driven subset and non-goals.
- [x] Add MySQL-runtime expectation script for this slice.
- [ ] Extend grouped aggregate planning with descriptor projection columns.
- [ ] Add primary-key coverage validation using catalog descriptors.
- [ ] Add wildcard expansion validation for grouped projections.
- [ ] Extend grouped `ORDER BY` validation for selected and unselected
  primary-key-dependent descriptor columns.
- [ ] Generate SQLite SQL from validated descriptor projections.
- [ ] Return projected descriptor values followed by aggregate values.
- [ ] Add C runtime tests for success, diagnostics, persistence, and
  independent handles.
- [ ] Update compatibility documentation.
- [ ] Run focused grouped/query lifecycle tests, the MySQL expectation script,
  and `cmake --workflow --preset check`.
- [ ] Review, commit, and push.
