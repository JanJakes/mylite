# Baseline UPDATE UNIX_TIMESTAMP Arithmetic Tasks

- [x] Verify MySQL 8.4.9 behavior for supported update assignments, string
      coercion, `NULL`, diagnostics, affected rows, and warnings.
- [x] Create `docs/specs/baseline-update-unix-timestamp-arithmetic/specs.md`
      with independently authored scope, grammar snippets, ownership
      boundaries, diagnostics, performance notes, and tests.
- [x] Extend update planning so admitted `UNIX_TIMESTAMP()` arithmetic values
      bypass same-column arithmetic rejection and validate target descriptors.
- [x] Reuse MyLite-owned statement timestamp, signed-64 delta evaluation, and
      descriptor conversion for update integer and nonbinary string targets.
- [x] Add C coverage for successful updates, WordPress text target coercion,
      `WHERE`/`ORDER BY`/`LIMIT`, persistence, diagnostics, and cleanup.
- [x] Add and run MySQL 8.4.9 expectation coverage.
- [x] Update compatibility docs for the exact supported `UPDATE` assignment
      surface.
- [x] Run focused tests and `cmake --workflow --preset check`.
- [x] Perform final feature review, fix any findings, commit, and push.
