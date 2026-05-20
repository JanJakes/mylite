# Baseline WordPress Core DDL Fixtures Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  representative WordPress core table DDL, defaults, indexes, metadata, row
  counts, and warning counts.
- [x] Add a MySQL-runtime expectation script covering the fixture setup and
  metadata/readback behavior.
- [x] Add fast C runtime coverage for `wp_users`, `wp_options`, and
  `wp_postmeta` fixture setup, row defaults, index metadata, persistence,
  independent handles, and file-format invariants.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` with
  the exact fixture-coverage wording.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for fixture relevance, scope control, descriptor
  authority, MySQL 8.4.9 evidence, file-format safety, and test quality.
