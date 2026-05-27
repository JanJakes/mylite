# Baseline INFORMATION_SCHEMA SCHEMATA_EXTENSIONS Tasks

- [x] Verify MySQL 8.4.9 `SCHEMATA_EXTENSIONS` columns, default rows, status,
  and system metadata.
- [x] Specify MyLite's empty-options baseline and read-only option boundary.
- [x] Add MySQL expectation script for `SCHEMATA_EXTENSIONS`.
- [x] Add `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` to the synthetic table
  registry.
- [x] Emit built-in and catalog schema rows with empty `OPTIONS`.
- [x] Ensure `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` expose
  system-view metadata for `SCHEMATA_EXTENSIONS`.
- [x] Add focused C tests for rows, metadata, filters, aliases, and diagnostics.
- [x] Update compatibility docs.
- [x] Run focused CTests and the MySQL expectation script.
- [x] Run `git diff --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, and push.

Deferred:

- `ALTER SCHEMA ... READ ONLY` and persistent schema read-only metadata.
- Privilege filtering.
- Physical `information_schema` tables.
