# Baseline INFORMATION_SCHEMA INNODB_BUFFER_POOL_STATS Tasks

- [x] Verify MySQL 8.4.9 `INNODB_BUFFER_POOL_STATS` table shape, system
      metadata, column metadata, default dynamic row presence, and status
      behavior.
- [x] Specify MyLite's single zero-row decision and out-of-scope InnoDB
      buffer-pool accounting behavior.
- [x] Register `INNODB_BUFFER_POOL_STATS` in the synthetic information-schema
      table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all 32
      columns.
- [x] Append a deterministic `POOL_ID = 0` zero-valued buffer-pool row.
- [x] Keep values stable after MyLite tables, indexes, and rows are created.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
