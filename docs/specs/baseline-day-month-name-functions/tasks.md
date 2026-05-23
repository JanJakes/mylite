# Baseline Day And Month Name Functions Tasks

- [x] Capture MySQL 8.4.9 expectation script for the supported and deferred
  behavior.
- [x] Add parser and AST support for `DAYNAME()` and `MONTHNAME()`, including
  argument-count nodes and identifier productions.
- [x] Extend temporal runtime extraction with English weekday and month names
  while preserving current calendar-date warning and descriptor policies.
- [x] Add row-scalar planning/runtime admission for both functions over the
  current descriptor-backed calendar-date envelope.
- [x] Add parser and runtime C tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and temporal/query-expression compatibility
  details for only the supported subset.
- [x] Run the MySQL expectation script, focused parser/runtime CTests, and the
  full `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL evidence, architecture boundaries,
  warning behavior, descriptor authority, performance, docs, and scope control.
