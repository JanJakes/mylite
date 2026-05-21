# Baseline Non-Strict Implicit Default Families Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for non-strict implicit defaults across the remaining descriptor
  families.
- [x] Specify the independently authored MyLite scope, ownership boundaries,
  diagnostics, generated SQLite handling, and test expectations.
- [x] Add MySQL 8.4.9 expectation probes for the covered descriptor families
  and the verified `ENUM` exceptions.
- [x] Implement descriptor-driven conversion and warning fixes.
- [x] Add fast C runtime coverage for inserted, replaced, and updated values,
  warnings, affected rows, and byte-safe readback.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests and MySQL expectation probes.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority,
  warning counts, affected rows, cleanup, and compatibility docs.
- [x] Commit and push the completed feature to `origin/main`.
