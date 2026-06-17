# Baseline Tableless SELECT Expression Clauses Tasks

- [x] Specify source-free and `FROM DUAL` `WHERE` / `ORDER BY` / `LIMIT`
  behavior and limits.
- [x] Verify representative MySQL 8.4.9 runtime behavior.
- [x] Broaden no-source and `FROM DUAL` `SELECT` parser productions.
- [x] Add tableless row-scalar `ORDER BY` validation without physical sorting.
- [x] Convert stale tableless `WHERE TRUE` and `ORDER BY 1` negative tests.
- [x] Add positive alias, expression-key, `FROM DUAL`, and invalid-key tests.
- [x] Update compatibility documentation.
- [x] Run focused parser/runtime/MySQL verification.
- [x] Run release checks and review the final diff.
