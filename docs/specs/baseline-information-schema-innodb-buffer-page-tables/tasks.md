# Baseline INFORMATION_SCHEMA INNODB_BUFFER_PAGE Tables Tasks

- [x] Verify MySQL 8.4.9 `INNODB_BUFFER_PAGE` and
      `INNODB_BUFFER_PAGE_LRU` table shapes, system metadata, column metadata,
      default dynamic row presence, and status behavior.
- [x] Specify MyLite's empty-row decision and out-of-scope InnoDB buffer-pool
      page accounting behavior.
- [x] Register both views in the synthetic information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all 41 total
      columns.
- [x] Keep both direct reads empty, including after MyLite tables and indexes
      are created.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
