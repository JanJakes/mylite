# Baseline Read-Only System Variables Tasks

- [x] Review existing system-variable registry, scalar read, `SHOW VARIABLES`,
  fixed no-op `SET`, diagnostics, and statement-context patterns.
- [x] Verify MySQL 8.4.9 behavior for default/global scalar reads, `SHOW`
  rows, scope diagnostics, no-op global assignments, and read-only
  `innodb_read_only` assignment diagnostics.
- [x] Write the independently authored feature specification and compatibility
  scope.
- [x] Add MySQL-runtime expectation artifact for the user-visible behavior.
- [x] Add runtime registry entries, fixed scalar values, `SHOW` values, scope
  validation, and `SET` diagnostics.
- [x] Add runtime tests and CMake registration if a new binary is needed.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused system-variable verification, the MySQL expectation script,
  and `cmake --workflow --preset check`.
- [x] Self-review, commit, request subagent review, amend if needed, and push.
