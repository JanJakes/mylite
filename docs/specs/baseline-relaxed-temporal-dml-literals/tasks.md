# Baseline Relaxed Temporal DML Literals Tasks

- [x] Read project architecture, temporal predicate specs, temporal storage
  conversion code, diagnostics/result behavior, compatibility docs, and SQLite
  fork policy.
- [x] Research official MySQL 8.4 temporal literal and SQL-mode
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `DATE`, `DATETIME`, and
  `TIMESTAMP` storage/default strings with `T`, numeric offsets, trailing
  `Z` / `z`, strict modes, warning counts, warning severities, default
  rendering, and normalized readback.
- [x] Write the independently authored feature spec with ownership boundaries,
  grammar notes, conversion rules, diagnostics, performance notes, and test
  plan.
- [x] Add MySQL-runtime expectation script for the user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Implement descriptor-backed relaxed temporal storage/default conversion
  and warning generation.
- [x] Add focused C tests for DML/default behavior, strict diagnostics,
  warning rows, persistence, and unsupported forms.
- [x] Run focused temporal tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, warning surface, descriptor
  authority, performance, cleanup, scope control, and compatibility accuracy.
- [ ] Commit and push the slice.
