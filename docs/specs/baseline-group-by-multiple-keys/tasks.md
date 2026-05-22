# Baseline GROUP BY Multiple Keys Tasks

- [x] Research official MySQL 8.4 `SELECT`, aggregate, and `GROUP BY`
      behavior plus existing MyLite grouped aggregate architecture.
- [x] Record MySQL 8.4.9 runtime expectations for the admitted multiple-key
      grouped aggregate subset.
- [x] Add parser/AST support for comma-separated `GROUP BY` descriptor key
      lists.
- [x] Extend grouped aggregate planning to store and validate selected group
      key prefixes.
- [x] Extend grouped `HAVING`, grouped `ORDER BY`, SQL generation, result
      metadata, and row readback for multiple selected group keys.
- [x] Add fast C runtime/parser tests for successful multiple-key grouping and
      deterministic diagnostics.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run the MySQL expectation script, focused grouped/parser tests,
      `cmake --build --preset dev`, and `cmake --workflow --preset check`.
- [x] Review the feature for descriptor authority, generated SQL safety,
      performance, scope control, cleanup, and compatibility accuracy.
