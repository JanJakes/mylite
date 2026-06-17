# Baseline Parenthesized Row Arithmetic Predicates Tasks

- [x] Create independent spec and record MySQL 8.4.9 runtime observations.
- [x] Keep the Lemon grammar unchanged and implement a targeted parser retry
      for redundant row arithmetic predicate parentheses.
- [x] Reuse existing row-scalar arithmetic runtime planning and diagnostics.
- [x] Add parser/runtime/MySQL expectation tests for parenthesized comparison,
      grouped predicate, `IS`, `BETWEEN`, `IN`, `MOD()`, and tableless filters.
- [x] Update compatibility docs and previous deferred notes.
- [x] Run focused tests, full workflow checks, release-gate review, commit, and
      push.
