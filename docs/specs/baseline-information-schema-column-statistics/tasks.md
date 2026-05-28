# Baseline INFORMATION_SCHEMA COLUMN_STATISTICS Tasks

- [x] Verify MySQL 8.4.9 documentation, baseline rows, user object behavior,
  status, histogram future-work behavior, and system metadata.
- [x] Add MySQL expectation script for `COLUMN_STATISTICS`.
- [x] Add `COLUMN_STATISTICS` to the synthetic information-schema table
  definitions.
- [x] Keep the current slice rowless until histogram descriptors exist.
- [x] Add focused C runtime tests for queries, predicates, metadata, selected
  `information_schema` resolution, and diagnostics.
- [x] Update compatibility docs.
- [x] Run focused tests, MySQL expectation script, `git diff --check`, and the
  full `cmake --workflow --preset check`.
- [x] Review, fix findings, commit, and push.
