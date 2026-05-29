# Baseline INFORMATION_SCHEMA ST_SPATIAL_REFERENCE_SYSTEMS Tasks

- [x] Verify MySQL 8.4.9 `ST_SPATIAL_REFERENCE_SYSTEMS` table shape, system
      metadata, column metadata, row count, row categories, representative
      rows, and status behavior.
- [x] Specify MyLite's empty-row decision and out-of-scope EPSG/SRID catalog
      behavior.
- [x] Register `ST_SPATIAL_REFERENCE_SYSTEMS` in the synthetic
      information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all six
      columns.
- [x] Keep direct reads empty, including after MyLite spatial columns and
      spatial index metadata are created.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
