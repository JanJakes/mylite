# Baseline WordPress Posts And Comments DDL Fixtures Tasks

- [x] Verify MySQL 8.4.9 behavior for representative `wp_posts` and
  `wp_comments` DDL, warnings, metadata, simple row defaults, and insert counts.
- [x] Specify the fixture slice, ownership boundaries, non-goals, diagnostics,
  metadata behavior, and performance expectations.
- [x] Extend the MySQL expectation script with `wp_posts` and `wp_comments`.
- [x] Extend fast C runtime coverage for the two fixture tables, metadata,
  row defaults, persistence, independent handles, and preamble preservation.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` with
  exact expanded fixture wording.
- [x] Run focused tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for fixture relevance, scope control, descriptor
  authority, MySQL evidence, file-format safety, and test quality.
