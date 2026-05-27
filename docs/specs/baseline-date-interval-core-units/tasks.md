# Baseline DATE Interval Core Units Tasks

- [x] Read existing DATE interval, temporal-function, scalar projection, and row-scalar projection specs and tests.
- [x] Verify MySQL 8.4.9 behavior for admitted single-part interval units, quoted integer intervals, date-vs-datetime output shape, month-end clamp behavior, metadata, warnings, and overflow.
- [x] Write an independently authored feature spec with MyLite Lemon-syntax snippets.
- [x] Add MySQL expectation script for the admitted DATE interval core-unit surface.
- [x] Extend parser/AST support for DATE interval unit children while preserving native function spacing rules.
- [x] Extend scalar evaluation for admitted units and exact quoted integer interval values.
- [x] Extend row-scalar planning, SQLite callback binding, evaluation, and metadata for admitted units.
- [x] Add parser and runtime tests for successful values, diagnostics, metadata, row warnings, persistence/file invariants, and composition with existing row-scalar clauses.
- [x] Update compatibility documentation for the exact admitted subset.
- [x] Run targeted parser/runtime/MySQL expectation tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Run subagent release-gate review, fix findings, commit atomically, and push `main`.
