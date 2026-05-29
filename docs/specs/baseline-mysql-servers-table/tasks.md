# Baseline mysql.servers Table Tasks

- [x] Read the MyLite repository guidance, engineering standards,
      compatibility matrix, mysql schema docs, and related mysql system-table
      specs/tests.
- [x] Verify MySQL 8.4.9 behavior for direct reads, column metadata, primary
      key metadata, table-status metadata, and row-count behavior.
- [x] Write an independently authored feature spec with supported rows,
      limitations, ownership boundaries, and MySQL runtime evidence.
- [x] Add the MySQL-runtime expectation script for the supported servers table
      surface.
- [x] Implement MyLite-owned `mysql.servers` definition, empty reads, and
      table-status metadata.
- [x] Add a focused C runtime test and CMake registration.
- [x] Update the compatibility matrix and related mysql/information-schema/SHOW
      compatibility docs.
- [x] Run focused tests, the MySQL expectation script, diff whitespace checks,
      clang-tidy for the new test, and the full check workflow.
- [x] Review the final diff, fix findings, commit atomically, and push
      `origin/main`.
