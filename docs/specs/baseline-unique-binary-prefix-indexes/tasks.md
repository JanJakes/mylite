# Baseline Unique Binary Prefix Indexes Tasks

- [x] Pick the narrow report-backed feature slice.
- [x] Verify MySQL 8.4.9 behavior for unique binary prefix metadata, DML
  conflicts, `INSERT IGNORE`, ODKU, update conflicts, and diagnostic byte
  rendering.
- [x] Write an independently authored feature spec.
- [x] Add MySQL-runtime expectation script.
- [x] Remove deliberate planner rejections for unique binary prefix key parts.
- [x] Make duplicate-key diagnostic formatting byte-safe for binary prefix
  values from planned DML values and SQLite existing-row validation queries.
- [x] Add focused C runtime tests for metadata, DML, persistence, diagnostics,
  and independent handles.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused tests, MySQL expectations, full build, and check workflow.
- [x] Self-review, commit, reviewer subagent pass, amend if needed, and push.
