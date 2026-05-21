# Baseline Character Expression Defaults Tasks

## Design And Evidence

- [x] Create the feature spec under
  `docs/specs/baseline-character-expression-defaults/`.
- [x] Verify MySQL 8.4.9 behavior for generated `CHAR`/`VARCHAR` defaults,
  metadata, DML materialization, `ALTER` variants, generated `NULL`, and
  overlength materialization.
- [x] Add a MySQL-runtime expectation script for the feature.

## Implementation

- [x] Add a catalog default kind for generated character text defaults and a
  migration from the previous catalog schema version.
- [x] Finalize parenthesized string defaults for `CHAR`/`VARCHAR` descriptors
  without converting declared length at DDL time.
- [x] Materialize generated character defaults through descriptor conversion at
  DML time.
- [x] Render generated character defaults in `SHOW COLUMNS`,
  `INFORMATION_SCHEMA.COLUMNS`, and `SHOW CREATE TABLE`.
- [x] Preserve descriptor copying through `CREATE TABLE ... LIKE`, column
  modification, reopen, and independent handles.

## Tests And Docs

- [x] Extend fast C runtime tests for successful generated defaults,
  diagnostics, metadata, persistence, and catalog migration.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs for the exact
  supported subset.
- [x] Run the new MySQL expectation script.
- [x] Run targeted CTest entries.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `main`.
