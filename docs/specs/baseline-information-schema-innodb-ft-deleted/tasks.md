# Baseline INFORMATION_SCHEMA InnoDB FT Deleted Tables Tasks

- [x] Verify MySQL 8.4.9 `INNODB_FT_DELETED` and
      `INNODB_FT_BEING_DELETED` table shapes, metadata, default empty row
      sets, and status behavior.
- [x] Register both views in the synthetic information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for both
      `DOC_ID` columns.
- [x] Return the observed default empty row sets with no warnings.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      and file-backed handle safety.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
