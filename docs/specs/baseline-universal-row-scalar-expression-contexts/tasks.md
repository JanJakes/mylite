# Baseline Universal Row-Scalar Expression Contexts Tasks

- [x] Add a shared row-scalar context-attempt classifier.
- [x] Route SELECT projection, optional clauses, ORDER BY, predicates, UPDATE,
      and duplicate-key UPDATE through the shared classifier.
- [x] Widen non-`GROUP_CONCAT` aggregate row-scalar argument planning.
- [x] Preserve `AVG(expression)` result formatting with the existing
      `SUM(...), COUNT(...)` execution shape.
- [x] Add focused runtime coverage for expression contexts and aggregate
      arguments.
- [x] Update compatibility docs to replace generic context-gap wording where
      the routing gap is closed.
- [x] Run focused tests, diff checks, workflow checks, review, commit, and push.
