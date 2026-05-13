# Baseline Zero Temporal SQL Modes Tasks

- [x] Audit existing SQL-mode and temporal type support.
- [x] Verify MySQL 8.4.9 behavior for zero, partial-zero, invalid, and
      `ALLOW_INVALID_DATES` temporal conversions.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the supported mode matrix.
- [ ] Implement mode-aware `DATE`, `DATETIME`, and `TIMESTAMP` conversion.
- [ ] Add fast runtime tests for DML, defaults, predicates, persistence, and
      independent handles.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run focused MySQL expectations, build, focused CTest entries, and full
      `cmake --workflow --preset check`.
- [ ] Review the final diff with a subagent, fix findings, commit, push, and
      continue.
