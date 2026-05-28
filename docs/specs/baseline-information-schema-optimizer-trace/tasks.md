# Baseline INFORMATION_SCHEMA OPTIMIZER_TRACE Tasks

- [x] Verify MySQL 8.4.9 `OPTIMIZER_TRACE` table shape, metadata, default
      empty-row behavior, and dynamic row-producing trace behavior.
- [x] Register `OPTIMIZER_TRACE` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all four
      columns.
- [x] Keep MyLite user-row production empty until optimizer tracing and
      related session variables exist.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      and file-backed handle isolation.
- [x] Add a MySQL 8.4.9 expectation script documenting the observed behavior.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
