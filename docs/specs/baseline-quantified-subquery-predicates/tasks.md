# Baseline Quantified Subquery Predicates Tasks

- [x] Inventory existing `IN` / `EXISTS` subquery planning, descriptor source
      contexts, comparison-result postfix predicates, and compatibility gaps.
- [x] Verify MySQL 8.4.9 behavior for supported and intentionally deferred
      `ANY` / `SOME` / `ALL` forms with runtime probes.
- [x] Specify the independently authored MyLite grammar and runtime subset.
- [x] Add MySQL expectation script for supported behavior and selected
      MySQL diagnostics.
- [x] Add parser/AST support for descriptor-column quantified subquery
      predicate atoms and postfix comparison-result `IS` predicates.
- [x] Add descriptor-driven analyzer/planner support by reusing the existing
      one-column `IN` subquery envelope.
- [x] Generate quoted SQLite SQL that preserves MySQL three-valued quantified
      comparison semantics.
- [x] Add parameter binding for duplicated inner predicate probes.
- [x] Add C parser and runtime tests under `packages/libmylite/tests/`.
- [x] Update compatibility docs from unsupported to implemented limited
      support.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `git diff --check`, staging checks, and `cmake --workflow --preset check`.
- [x] Review the diff for MySQL equivalence, descriptor authority, no
      arbitrary SQLite pass-through, result semantics, cleanup, diagnostics,
      docs accuracy, performance, and test relevance.
