# Baseline Last Insert ID Expression Tasks

- [x] Specify the narrow `LAST_INSERT_ID(expr)` scalar and `DO` feature surface
  from official MySQL 8.4 documentation and observed MySQL 8.4.9 behavior.
- [x] Record scope boundaries for literal-only arguments, unsigned session
  storage, `NULL`, signed wrapping, diagnostics, and deferred table-backed
  sequence-emulation behavior.
- [x] Extend parser/AST support for one-argument and argument-count-error
  `LAST_INSERT_ID` forms.
- [x] Implement scalar runtime conversion and session-state mutation without
  catalog, storage, VFS, or SQLite changes.
- [x] Add C coverage for parser, scalar `SELECT`, `DO`, errors, session
  persistence boundaries, auto-increment interaction, and cleanup safety.
- [x] Extend the MySQL 8.4.9 expectation artifact for the new user-visible
  behavior and intentional deferrals.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Verify focused tests, the MySQL expectation artifact, and the full check
  workflow.
- [x] Review the final diff for architecture boundaries, MySQL compatibility,
  warning/error behavior, session-state correctness, and scope control.
