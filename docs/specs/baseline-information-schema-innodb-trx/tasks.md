# Baseline INFORMATION_SCHEMA INNODB_TRX Tasks

- [x] Verify MySQL 8.4.9 `INNODB_TRX` table shape, system metadata, column
      metadata, empty default rows, active-transaction dynamic rows, and status
      behavior.
- [x] Specify MyLite's metadata-only empty-row decision and out-of-scope
      transaction/lock instrumentation behavior.
- [x] Register `INNODB_TRX` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all 25
      columns.
- [x] Keep direct reads empty, including while a MyLite transaction is active.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
