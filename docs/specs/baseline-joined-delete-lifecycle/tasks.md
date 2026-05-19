# Baseline Joined DELETE Lifecycle Tasks

- [x] Create independently authored feature specification.
- [x] Add MySQL 8.4.9 expectation script for admitted syntax, diagnostics, and
      row-count behavior.
- [x] Extend parser/AST for the two narrow joined-delete forms.
- [x] Reuse descriptor-driven joined source, `ON`, and `WHERE` planning.
- [x] Resolve one delete target against joined sources with MySQL-compatible
      alias/default-schema behavior.
- [x] Generate rowid-selector SQLite `DELETE` with quoted identifiers and bound
      predicate parameters.
- [x] Preserve direct parent-side FK delete actions with the same joined target
      selector.
- [x] Add runtime lifecycle tests for success, diagnostics, persistence,
      preamble safety, FK actions, rowid shadowing, and independent handles.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, push, and run subagent release-gate review.

Out of scope for this slice:

- multi-target deletes;
- `target.*`;
- comma table references and nested/derived table references;
- joined-delete `ORDER BY` / `LIMIT`;
- `LOW_PRIORITY`, `QUICK`, `IGNORE`, `PARTITION`, CTEs, subqueries, triggers,
  privileges, and recursive cascades.
