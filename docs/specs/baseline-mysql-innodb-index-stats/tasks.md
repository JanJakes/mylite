# Baseline mysql.innodb_index_stats Tasks

- [x] Verify MySQL 8.4.9 runtime table shape, metadata, stable built-in rows,
      user-table index-statistics behavior, and `SET timestamp` behavior.
- [x] Define the limited read-only MyLite behavior, metadata query envelope,
      ownership boundaries, diagnostics, and compatibility gaps.
- [x] Add a MySQL expectation script for the runtime evidence.
- [x] Implement `SELECT` routing for `mysql.innodb_index_stats` and
      unqualified reads after `USE mysql`.
- [x] Synthesize built-in and descriptor-backed index-stat rows.
- [x] Add `INFORMATION_SCHEMA.COLUMNS` metadata rows for
      `mysql.innodb_index_stats`.
- [x] Add focused C runtime coverage and CMake/CTest registration.
- [x] Update compatibility documentation and the high-level matrix.
- [x] Run focused CTests, the MySQL expectation script, diff checks, and the
      full `cmake --workflow --preset check` workflow.
- [x] Review the final diff, fix findings, commit, and push.
