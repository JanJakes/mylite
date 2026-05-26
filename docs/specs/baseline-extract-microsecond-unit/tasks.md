# Baseline EXTRACT MICROSECOND Unit Tasks

- [x] Choose feature scope and slug.
- [x] Review existing `EXTRACT()` and `MICROSECOND()` specs, docs, runtime, and
  tests.
- [x] Verify MySQL 8.4.9 runtime behavior for positive, negative, rounded,
  date-only, invalid, and row-backed `EXTRACT(MICROSECOND FROM ...)`.
- [x] Specify the narrow supported grammar/runtime surface.
- [x] Implement a signed microsecond extract kind distinct from
  `MICROSECOND()`.
- [x] Extend MySQL expectation and C runtime coverage.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused MySQL/parser/runtime verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review and amend findings.
- [x] Commit and push to remote `main`.
