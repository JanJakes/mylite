# Baseline GROUP BY Multiple Keys Tasks

- [ ] Research official MySQL 8.4 `SELECT`, aggregate, and `GROUP BY`
      behavior plus existing MyLite grouped aggregate architecture.
- [ ] Record MySQL 8.4.9 runtime expectations for the admitted multiple-key
      grouped aggregate subset.
- [ ] Add parser/AST support for comma-separated `GROUP BY` descriptor key
      lists.
- [ ] Extend grouped aggregate planning to store and validate selected group
      key prefixes.
- [ ] Extend grouped `HAVING`, grouped `ORDER BY`, SQL generation, result
      metadata, and row readback for multiple selected group keys.
- [ ] Add fast C runtime/parser tests for successful multiple-key grouping and
      deterministic diagnostics.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run the MySQL expectation script, focused grouped/parser tests,
      `cmake --build --preset dev`, and `cmake --workflow --preset check`.
- [ ] Review the feature for descriptor authority, generated SQL safety,
      performance, scope control, cleanup, and compatibility accuracy.
