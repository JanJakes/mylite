# Baseline INFORMATION_SCHEMA Extension Attribute Tables Tasks

- [x] Verify MySQL 8.4.9 column shape, system metadata, user rows, view rows,
  constraint rows, and check-constraint omission.
- [x] Add MySQL expectation script for the three extension-attribute views.
- [x] Add `COLUMNS_EXTENSIONS`, `TABLES_EXTENSIONS`, and
  `TABLE_CONSTRAINTS_EXTENSIONS` to the synthetic information-schema table
  definitions.
- [x] Populate system metadata rows and descriptor-backed extension rows.
- [x] Add focused C runtime tests for queries, predicates, metadata, selected
  `information_schema` resolution, and diagnostics.
- [x] Update compatibility docs.
- [x] Run focused tests, MySQL expectation script, `git diff --check`, and the
  full `cmake --workflow --preset check`.
- [x] Review, fix findings, commit, and push.
