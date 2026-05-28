# Baseline INFORMATION_SCHEMA PROFILING Tasks

- [x] Verify MySQL 8.4.9 `PROFILING` table shape, metadata, default empty-row
      behavior, direct-read deprecation warning, `LIMIT 0` warning behavior,
      and dynamic row-producing deprecated profiling behavior.
- [x] Register `PROFILING` in the synthetic information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all 18
      columns.
- [x] Keep MyLite user-row production empty until statement profiling and
      related session variables exist.
- [x] Add the MySQL-shaped `INFORMATION_SCHEMA.PROFILING` deprecation warning
      for supported direct reads.
- [x] Add a focused C runtime test for row shape, metadata, warnings, query
      behavior, and file-backed handle isolation.
- [x] Add a MySQL 8.4.9 expectation script documenting the observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
