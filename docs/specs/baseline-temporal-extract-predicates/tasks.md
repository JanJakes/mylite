# Baseline Temporal Extract Predicates Tasks

- [x] Read existing temporal extractor, date/time function, and predicate planner specs and tests.
- [x] Verify official MySQL 8.4 documentation for temporal extractor functions and comparison predicates.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted temporal extractor predicate truth, comparison, `NULL`, warning, and ordering/limit cases.
- [x] Write an independently authored feature spec with MyLite Lemon-syntax snippets.
- [x] Add MySQL expectation script for the admitted temporal extractor predicate surface.
- [x] Extend predicate planning to admit numeric temporal extractor truth, comparison, and `IS [NOT] NULL` predicates over one descriptor table source.
- [x] Reuse descriptor-driven row-scalar temporal extractor SQL generation and parameter binding without row materialization.
- [x] Add runtime tests for successful filtering, `NULL`, warnings, alias/qualified columns, order/limit envelope, and diagnostics.
- [x] Update compatibility documentation for the exact admitted subset.
- [x] Widen numeric temporal extractor predicates to supported joined `SELECT`
  source envelopes, including grouped left-join WordPress taxonomy queries.
- [x] Widen numeric temporal extractor predicates to integer/boolean
  `BETWEEN` ranges used by WordPress date queries.
- [x] Run targeted runtime/MySQL expectation tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Run subagent release-gate review, fix findings, commit atomically, and push `main`.
