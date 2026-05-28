# Baseline INFORMATION_SCHEMA TABLESPACES_EXTENSIONS Tasks

- [x] Verify MySQL 8.4.9 column shape, baseline rows, user base-table rows,
  view/temp omission, status, and system metadata.
- [x] Add MySQL expectation script for `TABLESPACES_EXTENSIONS`.
- [x] Add `TABLESPACES_EXTENSIONS` to the synthetic information-schema table
  definitions.
- [x] Populate fixed baseline system rows and descriptor-backed base-table rows.
- [x] Add focused C runtime tests for queries, predicates, metadata, selected
  `information_schema` resolution, and diagnostics.
- [x] Update compatibility docs.
- [x] Run focused tests, MySQL expectation script, `git diff --check`, and the
  full `cmake --workflow --preset check`.
- [x] Review, fix findings, commit, and push.
