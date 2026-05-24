# Baseline Insert Ignore Select Auto Increment Tasks

- [x] Read project architecture, existing insert-select, keyed-target,
  auto-increment, foreign-key, compatibility, runtime, and test context.
- [x] Research official MySQL 8.4 `INSERT`, `INSERT ... SELECT`, and
  `AUTO_INCREMENT` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for table-backed
  `INSERT IGNORE ... SELECT` into auto-increment targets, duplicate skips,
  foreign-key skips, explicit values, generated values, affected rows,
  warnings, and `LAST_INSERT_ID()`.
- [x] Document the independently authored feature spec, ownership boundaries,
  runtime semantics, diagnostics, performance notes, and exact remaining
  auto-increment reserve-gap limitation.
- [x] Update compatibility documentation for the exact admitted subset.
- [x] Remove the planner rejection for table-backed `INSERT IGNORE ... SELECT`
  into supported auto-increment targets while preserving row-scalar and
  compound-source `IGNORE` rejections.
- [x] Add MySQL-runtime expectation coverage for the newly admitted behavior.
- [x] Add focused runtime C coverage for generated, explicit, skipped,
  warning, persistence, and preamble cases.
- [x] Run focused insert-select lifecycle tests.
- [x] Run the MySQL expectation script against MySQL 8.4.9.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority,
  performance, cleanup on failure, file-format safety, scope control, and
  compatibility accuracy.
- [x] Commit and push the completed feature.
