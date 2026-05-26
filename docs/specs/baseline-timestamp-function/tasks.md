# Baseline TIMESTAMP() Function Tasks

- [x] Verify MySQL 8.4.9 behavior for one-argument, two-argument, `NULL`,
  invalid-value, row-backed descriptor, and arity cases.
- [x] Write an independently authored feature spec with grammar snippets,
  supported scope, deferred behavior, diagnostics, runtime ownership, and test
  expectations.
- [x] Add parser/AST support for `TIMESTAMP(value)` and
  `TIMESTAMP(value, time_value)`.
- [x] Add MyLite-owned runtime conversion for canonical date/datetime and time
  values, including warning-producing invalid input, time clipping, and
  overflow behavior.
- [x] Register a row-scalar SQLite callback through MyLite's SQLite extension
  registration path.
- [x] Add no-source, `DUAL`, `DO`, row-scalar, diagnostics, warning, and reopen
  C tests.
- [x] Add a MySQL 8.4.9 expectation script for the supported user-visible
  behavior and deferred-shape evidence.
- [x] Update `COMPATIBILITY.md` and temporal/query/literal compatibility docs
  for only this limited `TIMESTAMP()` subset.
- [x] Run the MySQL expectation script, focused CTests, and
  `cmake --workflow --preset check`.
- [x] Review the feature diff for architecture boundaries, descriptor
  authority, SQLite API use, warning semantics, docs accuracy, and test
  relevance before commit.
