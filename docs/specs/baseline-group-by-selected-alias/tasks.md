# Baseline GROUP BY Selected Alias Tasks

- [x] Research official MySQL 8.4 `SELECT` / `GROUP BY` behavior and existing
      MyLite grouped aggregate architecture.
- [x] Record MySQL 8.4.9 runtime expectations for selected descriptor-column
      aliases, source-column-first ambiguity, duplicate aliases, and deferred
      expression aliases.
- [x] Extend grouped aggregate planning to resolve unqualified group keys
      through unique selected descriptor-column aliases after descriptor-column
      resolution fails.
- [x] Add fast C runtime tests for successful alias-backed grouping and
      deterministic diagnostics.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run the MySQL expectation script, focused grouped/runtime tests,
      `cmake --build --preset dev`, and `cmake --workflow --preset check`.
- [x] Review the feature for descriptor authority, generated SQL safety,
      performance, scope control, cleanup, and compatibility accuracy.
