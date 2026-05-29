# Baseline INFORMATION_SCHEMA FILES Tasks

- [x] Verify MySQL 8.4.9 `FILES` table shape, system metadata, column
      metadata, default rows, and status behavior.
- [x] Specify MyLite's static default-row decision and out-of-scope InnoDB/NDB
      file accounting behavior.
- [x] Register `FILES` in the synthetic information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all 38
      columns.
- [x] Add the six observed default InnoDB file rows.
- [x] Keep `FILES` rows stable after MyLite user tables and indexes are
      created.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
