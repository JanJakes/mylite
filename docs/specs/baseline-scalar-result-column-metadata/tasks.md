# Baseline Scalar Result Column Metadata Tasks

- [x] Choose feature scope and slug.
- [x] Read existing result metadata, scalar expression, row-scalar, JSON
  function, public API, and compatibility docs.
- [x] Verify official MySQL 8.4 metadata documentation and MySQL 8.4.9 runtime
  behavior for the selected metadata subset.
- [x] Write independently authored feature specification with ownership
  boundaries, scope, metadata rules, diagnostics, performance, and tests.
- [x] Add MySQL-runtime expectation script for scalar and row-scalar metadata.
- [x] Populate no-source and `DUAL` scalar result metadata for the supported
  subset.
- [x] Populate row-scalar descriptor-column and JSON-function metadata for the
  supported subset.
- [x] Extend fast C metadata tests.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [x] Run focused tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, amend findings, commit, push remote `main`, and run review agent.
