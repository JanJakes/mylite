# Baseline Joined UPDATE Lifecycle Tasks

- [x] Review existing single-table update, joined select, and joined delete
  architecture.
- [x] Verify joined update behavior against MySQL 8.4.9.
- [x] Write independently authored feature spec and grammar snippet.
- [x] Add MySQL 8.4.9 expectation script for user-visible behavior.
- [x] Extend parser/AST support for the admitted joined update form.
- [x] Implement descriptor-driven joined update planning and execution.
- [x] Preserve existing update assignment conversion, affected-row semantics,
  metadata stability, file format invariants, and direct FK actions.
- [x] Add parser and runtime lifecycle tests.
- [x] Update compatibility documentation.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, push, and run subagent release-gate review.

Out of scope for this slice:

- Multiple assignment targets or updating multiple tables.
- Joined-update `ORDER BY` / `LIMIT`.
- Modifiers, partitions, nested joins, derived tables, CTEs, and subqueries in
  joined predicates.
- General expression, column-to-column, arithmetic, or scalar-subquery
  assignments.
- Trigger, privilege, and recursive foreign-key behavior.
