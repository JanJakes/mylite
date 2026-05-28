# Baseline INFORMATION_SCHEMA VIEW_ROUTINE_USAGE Tasks

- [x] Verify MySQL 8.4.9 `VIEW_ROUTINE_USAGE` table shape, metadata, empty-row
      baseline, native-function omission, and stored-function dependency rows.
- [x] Register `VIEW_ROUTINE_USAGE` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all six
      columns.
- [x] Keep MyLite user-row production empty until stored routine descriptors
      and view-to-routine dependency analysis exist.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      empty view behavior, and file-backed handle isolation.
- [x] Add a MySQL 8.4.9 expectation script documenting the observed behavior.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
