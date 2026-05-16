# Baseline String Range Predicates Tasks

- [x] Read project architecture, string predicate/order specs, compatibility
  docs, predicate planner code, diagnostics/result behavior, and SQLite fork
  policy.
- [x] Research official MySQL 8.4 comparison and string comparison
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for descriptor string comparisons,
  `BETWEEN`, `IN`, `NULL` list semantics, trailing spaces, ISO-like text, DML
  affected rows, and warning counts.
- [x] Write the independently authored feature spec with ownership boundaries,
  grammar notes, semantics, diagnostics, performance notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Implement descriptor-backed string range and membership predicate
  planning.
- [x] Add focused C tests for query, aggregate, DML, persistence, preamble,
  and unsupported forms.
- [x] Run focused predicate tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the slice.
