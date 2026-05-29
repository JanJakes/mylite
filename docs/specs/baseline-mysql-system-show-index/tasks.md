# Baseline mysql System SHOW INDEX Tasks

- [x] Read project guidance, compatibility docs, and existing mysql stats and
      SHOW INDEX specs/tests.
- [x] Verify MySQL 8.4.9 behavior for `SHOW INDEX`, `SHOW INDEXES`,
      `SHOW KEYS`, selected-schema forms, and `WHERE` over
      `mysql.innodb_table_stats` and `mysql.innodb_index_stats`.
- [x] Specify the supported target forms, result rows, diagnostics, and
      architecture boundary.
- [x] Add a MySQL-runtime expectation script for the slice.
- [x] Reuse MyLite-owned mysql system key metadata to render `SHOW INDEX`
      rows for the two supported stats tables.
- [x] Add focused C runtime coverage and CMake registration.
- [x] Update compatibility documentation.
- [x] Run focused tests, MySQL expectation script, whitespace checks, full
      `cmake --workflow --preset check`, and final review.
