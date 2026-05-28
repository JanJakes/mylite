# Baseline INFORMATION_SCHEMA INNODB_TEMP_TABLE_INFO Tasks

- [x] Verify MySQL 8.4.9 `INNODB_TEMP_TABLE_INFO` table shape, metadata,
      default row count, dynamic temporary-table behavior, and status
      behavior.
- [x] Register the view in the synthetic information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all four
      columns.
- [x] Return the observed default empty row set with no warnings.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      and file-backed handle safety.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
