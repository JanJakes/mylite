# Baseline BIT and YEAR Expression Defaults Tasks

- [x] Verify MySQL 8.4.9 behavior for `BIT` and `YEAR` expression defaults.
- [x] Specify the exact MyLite-owned grammar, planning, conversion, metadata,
  storage, and diagnostic surface.
- [x] Update MySQL expectation scripts for the new supported behavior.
- [x] Extend runtime tests for successful `BIT` and `YEAR` expression defaults.
- [x] Extend runtime tests for diagnostics, persistence, descriptor copies, and
  public result conventions.
- [x] Implement planner/catalog/runtime materialization support without public
  ABI changes or SQLite fork patches.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused tests, MySQL expectation scripts, and the full check workflow.
- [x] Review the final diff, fix findings, commit, and push.
