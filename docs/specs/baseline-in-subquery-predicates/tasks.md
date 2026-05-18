# Baseline IN Subquery Predicates Tasks

- [x] Inventory existing literal `IN` predicate support, `EXISTS` subquery
      support, predicate planning, descriptor source contexts, and
      compatibility gaps.
- [x] Verify MySQL 8.4.9 behavior for supported and intentionally deferred
      `IN (subquery)` / `NOT IN (subquery)` forms with runtime probes.
- [x] Specify the independently authored MyLite grammar and runtime subset.
- [x] Add MySQL expectation script for supported behavior and deferred wider
      MySQL forms.
- [ ] Add parser/AST support for `qualified_identifier [NOT] IN
      (select_statement)` predicate atoms.
- [ ] Add descriptor-driven analyzer/planner support for one-table inner
      `IN` subqueries with one explicit descriptor selected column.
- [ ] Add limited correlated integer column comparison planning inside inner
      `WHERE` predicates by reusing the `EXISTS` correlation envelope.
- [ ] Generate quoted SQLite `IN (SELECT ...)` SQL with stable source aliases
      and bound inner predicate parameters.
- [ ] Add cleanup/deinit coverage for nested predicate plans.
- [ ] Add C parser and runtime tests under `packages/libmylite/tests/`.
- [ ] Update compatibility docs from unsupported to implemented limited
      support.
- [ ] Run focused parser/runtime/subquery/select-where tests and the MySQL
      expectation script.
- [ ] Run `cmake --build --preset dev` and `cmake --workflow --preset check`.
- [ ] Review the diff for descriptor authority, no arbitrary SQLite
      pass-through, result semantics, performance, cleanup, diagnostics, docs
      accuracy, and test relevance.
