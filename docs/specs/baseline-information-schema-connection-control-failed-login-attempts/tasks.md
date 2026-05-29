# Baseline INFORMATION_SCHEMA CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS Tasks

- [x] Verify MySQL 8.4.9 plugin activation, table shape, system metadata,
      column metadata, empty-row behavior, and status behavior.
- [x] Specify MyLite's unconditional empty-view decision and out-of-scope
      connection-control plugin/account behavior.
- [x] Register `CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` in the synthetic
      information-schema table registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for both columns.
- [x] Keep direct reads empty, including after ordinary schema and table
      activity.
- [x] Add a focused C runtime test and CMake registration.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
