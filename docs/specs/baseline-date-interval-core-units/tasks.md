# Baseline DATE Interval Core Units Tasks

- [x] Read existing DATE interval, temporal-function, scalar projection, and row-scalar projection specs and tests.
- [x] Verify MySQL 8.4.9 behavior for admitted single-part interval units, quoted integer intervals, date-vs-datetime output shape, month-end clamp behavior, metadata, warnings, and overflow.
- [x] Write an independently authored feature spec with MyLite Lemon-syntax snippets.
- [x] Add MySQL expectation script for the admitted DATE interval core-unit surface.
- [ ] Extend parser/AST support for DATE interval unit children while preserving native function spacing rules.
- [ ] Extend scalar evaluation for admitted units and exact quoted integer interval values.
- [ ] Extend row-scalar planning, SQLite callback binding, evaluation, and metadata for admitted units.
- [ ] Add parser and runtime tests for successful values, diagnostics, metadata, row warnings, persistence/file invariants, and composition with existing row-scalar clauses.
- [ ] Update compatibility documentation for the exact admitted subset.
- [ ] Run targeted parser/runtime/MySQL expectation tests.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Run subagent release-gate review, fix findings, commit atomically, and push `main`.
