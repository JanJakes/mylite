# Baseline Row-Scalar Truth Predicates Tasks

- [x] Research official MySQL 8.4 expression/truth behavior and probe MySQL
  8.4.9 runtime results.
- [x] Specify supported syntax, semantics, architecture, and exclusions.
- [x] Add parser support for bare row-scalar predicate atoms.
- [x] Add planner support for generic row-scalar truth predicates.
- [x] Extend MySQL expectation coverage for row-scalar truth predicates.
- [x] Extend focused MyLite C runtime coverage for row-scalar truth predicates.
- [x] Update compatibility docs to remove stale bare-truth exclusions for the
  newly supported predicate surface.
- [x] Run focused MySQL/C/parser verification and the full check workflow.
- [x] Perform release-gate review, fix findings, commit, and push.
