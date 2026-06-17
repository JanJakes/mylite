# Baseline Row Arithmetic Predicates Tasks

- [x] Create independent spec and record MySQL 8.4.9 runtime observations.
- [x] Add narrow MyLite grammar for row arithmetic predicate operands.
- [x] Reuse row-scalar arithmetic planning for truth, comparison, `IS`, `BETWEEN`,
      and `IN` predicates.
- [x] Add row-scalar `IS boolean` SQL lowering without duplicate parameter
      binding.
- [x] Update parser/runtime/MySQL expectation tests.
- [x] Update compatibility docs and remove stale deferred notes.
- [x] Document parenthesized top-level arithmetic predicate roots as follow-up
      work.
- [x] Run focused tests, full workflow checks, release-gate review, commit, and
      push.
