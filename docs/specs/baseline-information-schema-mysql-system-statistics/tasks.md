# Baseline INFORMATION_SCHEMA mysql System STATISTICS Tasks

- [x] Read the MyLite repository guidance, engineering standards,
      compatibility matrix, mysql schema metadata docs, and related SHOW INDEX
      specs/tests.
- [x] Verify MySQL 8.4.9 behavior for `INFORMATION_SCHEMA.STATISTICS` rows on
      `mysql.innodb_table_stats` and `mysql.innodb_index_stats`.
- [x] Write an independently authored feature spec with supported rows,
      limitations, ownership boundaries, and MySQL runtime evidence.
- [x] Add the MySQL-runtime expectation script for the supported statistics
      rows.
- [x] Implement MyLite-owned `INFORMATION_SCHEMA.STATISTICS` rows for the two
      supported synthetic mysql tables.
- [x] Add a focused C runtime test and CMake registration.
- [x] Update the compatibility matrix and mysql/information-schema
      compatibility docs.
- [x] Run focused tests, the MySQL expectation script, diff whitespace checks,
      clang-tidy for the new test, and the full check workflow.
- [x] Review the final diff, fix findings, commit atomically, and push
      `origin/main`.
