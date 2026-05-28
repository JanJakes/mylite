# Baseline INFORMATION_SCHEMA InnoDB Tablespace Metadata Tasks

- [x] Verify MySQL 8.4.9 `INNODB_DATAFILES` and
      `INNODB_TABLESPACES_BRIEF` table shape, metadata, default rows, and
      status behavior.
- [x] Register both views in the synthetic information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all seven
      columns across both views.
- [x] Emit the observed four-row default datafile and tablespace-brief
      catalogs.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      and file-backed handle safety.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
