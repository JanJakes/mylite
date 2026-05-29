# Baseline INFORMATION_SCHEMA InnoDB Session Temp Tablespaces Tasks

- [x] Verify MySQL 8.4.9 `INNODB_SESSION_TEMP_TABLESPACES` table shape,
      column metadata, baseline rows, temporary-table allocation observation,
      and status behavior.
- [x] Specify MyLite's synthetic baseline-row decision and out-of-scope
      physical InnoDB temporary tablespaces.
- [x] Register `INNODB_SESSION_TEMP_TABLESPACES` in the synthetic
      information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all six
      columns.
- [x] Emit the observed ten-row baseline session temporary tablespace catalog,
      with the active intrinsic row keyed to the current connection id.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
