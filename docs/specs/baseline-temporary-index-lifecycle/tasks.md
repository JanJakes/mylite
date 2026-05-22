# Baseline Temporary Index Lifecycle Tasks

- [x] Verify MySQL 8.4.9 behavior for temporary-table index creation, drop,
      metadata visibility, shadowing, affected rows, warnings, and diagnostics.
- [x] Specify the supported grammar, resolver, planner, temporary-catalog,
      physical SQLite, result, diagnostic, and compatibility boundaries.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Add temporary-catalog append/delete index descriptor APIs.
- [x] Route index add/drop planning through visible temporary table resolution
      while preserving the persistent path.
- [x] Implement temporary index add/drop physical mutation and affected-row
      reporting.
- [x] Add fast C runtime tests and CMake registration.
- [x] Update compatibility docs for the exact supported temporary index subset.
- [x] Run focused MySQL, build, CTest, and full check workflow.
- [x] Run feature review, fix findings, commit, and push to `origin/main`.
