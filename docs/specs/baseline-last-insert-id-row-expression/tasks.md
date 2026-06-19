# Baseline Last Insert ID Row Expression Tasks

- [x] Research official MySQL 8.4 documentation and observed MySQL 8.4.9
      behavior for row-backed `LAST_INSERT_ID(expr)`.
- [x] Specify supported row projection, predicate, ordering, and sequence-update
      contexts, plus explicit conversion and stored-program gaps.
- [x] Implement row-scalar planning and private SQLite UDF execution.
- [x] Add MySQL expectation and C runtime coverage.
- [x] Update compatibility docs.
- [x] Run focused MySQL/runtime checks, `git diff --check`, staged diff check,
      and `cmake --workflow --preset check`.
- [x] Release-gate review, fix findings, commit, and push.
