# Baseline Parenthesized Current Timestamp Defaults Tasks

- [x] Choose feature slug `baseline-parenthesized-current-timestamp-defaults`.
- [x] Read project guidance, engineering standards, compatibility matrix, and
      existing current timestamp/date/time default specs and tests.
- [x] Research official MySQL 8.4 documentation for temporal automatic
      defaults and default expressions.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted parenthesized current
      timestamp defaults, metadata, materialization, alter paths, and deferred
      broader expression defaults.
- [x] Write the independently authored feature specification with MyLite
      grammar snippets and ownership boundaries.
- [x] Add a MySQL expectation script for the admitted and deferred behavior.
- [x] Implement descriptor validation/finalization for parenthesized current
      timestamp defaults.
- [x] Add focused C runtime tests for create/add/set/modify/change/like,
      diagnostics, persistence, file safety, and independent handles.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused MySQL expectations, focused CTests, build, and full
      `cmake --workflow --preset check`.
- [x] Review the final diff, commit, request subagent review, amend if needed,
      and push to `origin/main`.
