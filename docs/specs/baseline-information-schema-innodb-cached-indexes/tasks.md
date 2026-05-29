# Baseline INFORMATION_SCHEMA INNODB_CACHED_INDEXES Tasks

- [x] Verify MySQL 8.4.9 `INNODB_CACHED_INDEXES` table shape, system metadata,
      column metadata, default dynamic rows, and status behavior.
- [x] Specify MyLite's metadata-only empty-row decision and out-of-scope
      InnoDB buffer-pool page accounting behavior.
- [x] Register `INNODB_CACHED_INDEXES` in the synthetic information-schema
      table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all three
      columns.
- [x] Keep direct reads empty, including after MyLite tables and indexes are
      created.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
