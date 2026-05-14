# Baseline LEFT JOIN SELECT Tasks

- [x] Review existing inner-join, descriptor select, table alias, predicate,
  ordering, temporary-table, and catalog behavior.
- [x] Verify MySQL 8.4.9 behavior for `LEFT JOIN`, `LEFT OUTER JOIN`, required
  join specifications, null extension, wildcard column order, predicate
  placement, diagnostics, warning count, and row-count state.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script for the supported user-visible
  behavior and scoped-out MySQL-accepted forms.
- [x] Extend parser/AST support for `LEFT JOIN` and `LEFT OUTER JOIN` while
  preserving existing inner/cartesian join behavior.
- [x] Extend descriptor-driven join planning with a join kind and require `ON`
  for left outer joins.
- [x] Generate `LEFT JOIN` physical SQLite SQL from stable descriptor-owned
  physical table names and internal aliases.
- [x] Add runtime tests for left-row preservation, right-side `NULL` extension,
  predicate placement, ordering, limits, aliases, schema qualification,
  temporary shadowing, persistence, diagnostics, and file-format invariants.
- [x] Update compatibility docs for the exact limited left-join subset.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff with a subagent, amend any findings, commit, and
  push to remote `main`.
