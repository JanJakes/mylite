# Baseline INFORMATION_SCHEMA ST_UNITS_OF_MEASURE Tasks

- [x] Verify MySQL 8.4.9 `ST_UNITS_OF_MEASURE` table shape, metadata, static
      rows, and status behavior.
- [x] Register `ST_UNITS_OF_MEASURE` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all four
      columns.
- [x] Emit the observed 47-row static spatial unit catalog.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      and file-backed handle safety.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
