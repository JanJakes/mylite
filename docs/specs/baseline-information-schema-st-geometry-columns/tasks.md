# Baseline INFORMATION_SCHEMA ST_GEOMETRY_COLUMNS Tasks

- [x] Verify MySQL 8.4.9 `ST_GEOMETRY_COLUMNS` table shape, metadata, default
      empty-row behavior, and rows for no-SRID spatial columns.
- [x] Register `ST_GEOMETRY_COLUMNS` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all seven
      columns.
- [x] Emit descriptor-backed rows for supported persistent-table spatial
      columns.
- [x] Keep `SRS_NAME` and `SRS_ID` as SQL `NULL` until MyLite implements SRID
      attributes and SRS catalogs.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      and file-backed handle isolation.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md`,
      `docs/compatibility/metadata-information-schema.md`,
      `docs/compatibility/sql-indexes-constraints.md`, and
      `docs/compatibility/type-system-literals-conversion.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
