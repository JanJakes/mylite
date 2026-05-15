# Baseline EXISTS Subquery Predicates Tasks

- [x] Inventory existing scalar/table-backed subquery support, predicate
      planning, descriptor source contexts, and compatibility gaps.
- [x] Verify MySQL 8.4.9 behavior for supported and intentionally deferred
      `EXISTS` / `NOT EXISTS` forms with runtime probes.
- [x] Specify the independently authored MyLite grammar and runtime subset.
- [x] Add MySQL expectation script for supported behavior and deferred wider
      MySQL forms.
- [x] Mark compatibility docs as designed/pending.
- [x] Add parser/AST support for `EXISTS (select_statement)` predicate atoms.
- [x] Add descriptor-driven analyzer/planner support for tableless, `DUAL`,
      and one-table inner `EXISTS` subqueries.
- [x] Add limited correlated integer column comparison planning inside inner
      `WHERE` predicates.
- [x] Generate quoted SQLite `EXISTS` SQL with stable source aliases and bound
      predicate/limit parameters.
- [x] Add cleanup/deinit coverage for nested predicate plans.
- [x] Add C parser and runtime tests under `packages/libmylite/tests/`.
- [x] Update compatibility docs from designed/pending to implemented limited
      support.
- [x] Run focused parser/runtime/subquery/select-where tests and the MySQL
      expectation script.
- [x] Run `cmake --build --preset dev` and `cmake --workflow --preset check`.
- [x] Review the diff for descriptor authority, no arbitrary SQLite
      pass-through, result semantics, performance, cleanup, diagnostics, docs
      accuracy, and test relevance.
