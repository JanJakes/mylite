# Baseline Seeded RAND Function Tasks

- [x] Verify seeded `RAND(seed)` behavior against MySQL 8.4.9 for exact values,
  labels, row counts, warnings, argument-count diagnostics, and deferred
  table-backed behavior.
- [x] Specify the exact supported seed-literal subset and deferred expression
  surface in `specs.md`.
- [x] Add parser/AST support for a one-argument seeded RAND node while
  preserving the native argument-count marker.
- [x] Add MyLite-owned seeded RAND conversion and recurrence logic for admitted
  no-source/`DUAL`/`DO` scalar calls.
- [x] Add runtime coverage for exact seeded values, metadata/status behavior,
  unsupported seed forms, table-backed rejection, and file-format safety.
- [x] Update compatibility documentation with limited seeded RAND wording.
- [x] Run MySQL expectation tests, focused parser/runtime CTests, and
  `cmake --workflow --preset check`.
- [ ] Review the diff for MySQL parity, scope control, architecture boundaries,
  performance, file-format safety, and test relevance.
