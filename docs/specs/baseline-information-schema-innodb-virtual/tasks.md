# Baseline INFORMATION_SCHEMA InnoDB Virtual Tasks

- [x] Verify MySQL 8.4.9 `INNODB_VIRTUAL` table shape, column metadata,
      virtual dependency rows, duplicate-reference behavior, constant-column
      omission, per-table virtual ordinals, and status behavior.
- [x] Specify MyLite's descriptor-backed dependency-row decision and
      out-of-scope physical InnoDB dictionary behavior.
- [x] Register `INNODB_VIRTUAL` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all three
      columns.
- [x] Emit descriptor-backed virtual generated-column dependency rows with
      MySQL-observed `POS` and `BASE_POS` encoding.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
