# Baseline INFORMATION_SCHEMA InnoDB Tablestats Tasks

- [x] Verify MySQL 8.4.9 `INNODB_TABLESTATS` table shape, column metadata,
      user-table row behavior, auto-increment values, and status behavior.
- [x] Specify MyLite's synthetic descriptor-backed stats decision and
      out-of-scope physical InnoDB statistics.
- [x] Register `INNODB_TABLESTATS` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all nine
      columns.
- [x] Emit descriptor-backed user-table rows with exact row counts and
      deterministic synthetic stats placeholders.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
