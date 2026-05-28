# Baseline INFORMATION_SCHEMA Role Grant Tables Tasks

- [x] Verify MySQL 8.4.9 documentation, baseline rows, status, and system
  metadata for role grant views.
- [x] Add MySQL expectation script for `ROLE_COLUMN_GRANTS`,
  `ROLE_ROUTINE_GRANTS`, and `ROLE_TABLE_GRANTS`.
- [x] Add the three role grant views to the synthetic information-schema table
  definitions.
- [x] Keep the current slice rowless until role and grant descriptors exist.
- [x] Add focused C runtime tests for queries, predicates, metadata, selected
  `information_schema` resolution, and diagnostics.
- [x] Update compatibility docs.
- [x] Run focused tests, MySQL expectation script, `git diff --check`, and the
  full `cmake --workflow --preset check`.
- [x] Review, fix findings, commit, and push.
