# Baseline INFORMATION_SCHEMA InnoDB Tablespaces Tasks

- [x] Verify MySQL 8.4.9 `INNODB_TABLESPACES` table shape, column metadata,
      baseline rows, and status behavior.
- [x] Register `INNODB_TABLESPACES` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all 15
      columns.
- [x] Emit the observed five-row baseline tablespace catalog.
- [x] Extend the focused C runtime test for row shape, metadata, query
      behavior, selected-schema lookup, and file-backed handle safety.
- [x] Extend the MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
