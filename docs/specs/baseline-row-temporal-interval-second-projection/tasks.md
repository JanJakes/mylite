# Baseline Row Temporal Interval SECOND Projection Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior.
- [x] Specify the supported row-backed temporal interval-second projection subset.
- [x] Add MySQL-runtime expectation coverage for the newly introduced behavior.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Implement row-scalar planner support for `DATE_ADD`, `DATE_SUB`, `ADDDATE`, and `SUBDATE`.
- [x] Add a MyLite-owned row-backed SQLite scalar callback for interval-second arithmetic.
- [x] Add focused C runtime tests and CTest registration.
- [x] Run focused MySQL and C tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, amend issues, commit, and push `main`.
