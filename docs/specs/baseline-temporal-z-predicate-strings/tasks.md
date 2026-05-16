# Baseline Temporal Z Predicate Strings Tasks

- [x] Read project architecture, temporal type specs, predicate conversion
  code, diagnostics/result behavior, compatibility docs, and SQLite fork
  policy.
- [x] Research official MySQL 8.4 temporal literal documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for trailing `Z` / `z` predicate
  strings, warning messages, warning counts, session time-zone behavior, and
  malformed wider trailing garbage.
- [x] Write the independently authored feature spec with ownership boundaries,
  grammar notes, conversion rules, diagnostics, performance notes, and test
  plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Implement descriptor-backed trailing-`Z` predicate conversion and
  warning generation for `DATETIME` and `TIMESTAMP`.
- [x] Add focused C tests for datetime/timestamp row behavior, warnings,
  DML predicate reuse, and unsupported forms.
- [x] Run focused datetime/timestamp tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, warning surface, descriptor
  authority, performance, cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the slice.
