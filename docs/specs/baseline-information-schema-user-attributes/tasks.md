# Baseline INFORMATION_SCHEMA USER_ATTRIBUTES Tasks

- [x] Verify MySQL 8.4.9 documentation, baseline rows, diagnostics, and
  system metadata for `USER_ATTRIBUTES`.
- [x] Add a MySQL expectation script for the supported root-account baseline.
- [x] Add the synthetic `USER_ATTRIBUTES` table definition and embedded
  `root@%` row.
- [x] Add focused C runtime tests for queries, predicates, metadata, selected
  `information_schema` resolution, and diagnostics.
- [x] Update compatibility docs.
- [x] Run focused tests, MySQL expectation script, `git diff --check`, and the
  full `cmake --workflow --preset check`.
- [x] Review, fix findings, commit, and push.
