# Baseline Joined Aggregate SELECT Tasks

- [x] Review existing joined SELECT, grouped aggregate, HAVING, predicate,
  ordering, limit, descriptor, temporary-table, and result behavior.
- [x] Verify MySQL 8.4.9 behavior for joined grouped aggregates, especially
  left-join null-extension with `COUNT(*)` versus `COUNT(right_column)`.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for supported behavior and
  deliberately deferred wider forms.
- [x] Extend the grouped aggregate planner to accept the existing two-source
  joined source envelope without changing public ABI.
- [x] Resolve group, aggregate, predicate, having, and order columns through the
  joined descriptor resolver and track source indexes.
- [x] Generate grouped aggregate SQL from descriptor-owned physical table names,
  quoted generated source aliases, and bound parameters.
- [x] Add runtime tests for joined grouped success cases, diagnostics,
  persistence, and file-format invariants.
- [x] Update compatibility docs for the exact joined grouped aggregate subset.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff with a subagent, amend any findings, commit, and
  push to remote `main`.
