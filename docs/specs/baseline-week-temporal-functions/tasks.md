# Baseline Week Temporal Functions Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing temporal function
  specs.
- [x] Verify official MySQL 8.4 documentation for `WEEK()`, `WEEKDAY()`,
  `WEEKOFYEAR()`, `YEARWEEK()`, and `default_week_format`.
- [x] Probe MySQL 8.4.9 runtime behavior for supported week calculations,
  modes, omitted mode behavior, zero and partial dates, labels, warnings, row
  count, and syntax errors.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Extend lexer/parser/AST support for the four functions and wrong arities.
- [x] Extend the MyLite-owned temporal scalar runtime helper with week
  calculations and mode-aware execution.
- [x] Wire scalar and row-scalar analyzer/planner/execution paths.
- [x] Add focused C runtime tests and parser coverage.
- [x] Update compatibility documentation.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.
