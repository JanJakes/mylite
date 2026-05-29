# Baseline mysql System Stats Table Status Tasks

- [x] Read the MyLite repository guidance, engineering standards,
      compatibility matrix, built-in schema directory spec, and related mysql
      stats table specs/tests.
- [x] Verify MySQL 8.4.9 behavior for `INFORMATION_SCHEMA.TABLES` and
      `SHOW TABLE STATUS` rows on `mysql.innodb_table_stats` and
      `mysql.innodb_index_stats`.
- [x] Write an independently authored feature spec with supported rows,
      limitations, ownership boundaries, and MySQL runtime evidence.
- [x] Add the MySQL-runtime expectation script for the supported status rows.
- [x] Implement MyLite-owned status metadata for the two supported synthetic
      mysql stats tables.
- [x] Add a focused C runtime test and CMake registration.
- [x] Update the compatibility matrix and mysql/information-schema
      compatibility docs.
- [x] Run focused tests, the MySQL expectation script, diff whitespace checks,
      clang-tidy for the new test, and the full check workflow.
- [x] Review the final diff, fix findings, commit atomically, and push
      `origin/main`.
