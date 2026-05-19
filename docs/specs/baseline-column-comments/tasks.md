# Baseline Column Comments Tasks

- [x] Verify MySQL 8.4.9 syntax, metadata, duplicate-comment, alter, clone,
      and length behavior.
- [x] Write the independently authored feature spec.
- [x] Add a MySQL 8.4.9 expectation script for supported user-visible behavior.
- [x] Extend parser/AST support for `COMMENT 'string'` column attributes.
- [x] Add durable catalog column-comment metadata and migration.
- [x] Thread comments through create, temporary, clone, add, modify, change,
      rename, and rebuild descriptor paths.
- [x] Render comments through `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, and
      `INFORMATION_SCHEMA.COLUMNS`.
- [x] Add focused runtime/parser tests and CMake registration.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused build/tests, MySQL expectation script, and
      `cmake --workflow --preset check`.
- [x] Review the feature, amend fixes if needed, commit, and push.
