# Baseline INTERSECT / EXCEPT Select Lifecycle Tasks

- [x] Read project architecture, query-expression compatibility docs,
  existing `UNION` lifecycle specs/tests, parser/AST sources, runtime compound
  execution, insert-select compound sources, result APIs, and SQLite fork
  policy.
- [x] Research official MySQL 8.4 set-operation, `INTERSECT`, `EXCEPT`, and
  `UNION` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for distinct/all semantics,
  duplicate counts, `NULL`, collation behavior, result labels, diagnostics,
  `ROW_COUNT()`, warnings, branch clause boundaries, global order/limit, and
  mixed-operator precedence.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, row comparison, multiset handling,
  unsupported surfaces, diagnostics, performance notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend AST/parser support for `INTERSECT` and `EXCEPT` compound terms
  while preserving existing `UNION` behavior.
- [x] Add runtime support for homogeneous `INTERSECT` / `EXCEPT` result-set
  execution, mixed-operator rejection, distinct/all semantics, and result
  cleanup on failure.
- [x] Keep `INSERT ... SELECT` compound sources limited to `UNION` and reject
  `INTERSECT` / `EXCEPT` deterministically.
- [x] Add focused parser/runtime C tests for success paths, diagnostics, result
  metadata, read-only persistence, independent handles, and unsupported forms.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, result metadata, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
