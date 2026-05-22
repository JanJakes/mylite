# Baseline Replace Select Keyed Targets Tasks

- [x] Read current architecture, compatibility, `REPLACE ... SELECT`, keyed
  `REPLACE`, insert-select keyed target, parser, runtime, and test context.
- [x] Verify official MySQL 8.4 documentation for `REPLACE`, `INSERT ...
  SELECT`, and `ROW_COUNT()`.
- [x] Probe MySQL 8.4.9 runtime behavior for primary-key, unique-key,
  composite-key, prefix-key, nullable unique-key, multiple-conflict,
  auto-increment, same-table, and foreign-key replacement cases.
- [x] Write the independent feature spec with ownership boundaries, grammar
  snippet, physical SQLite handling, diagnostics, and tests.
- [x] Extend MySQL runtime expectation script for keyed `REPLACE ... SELECT`.
- [x] Update compatibility documentation for the expanded limited subset.
- [x] Reuse the existing materialized `REPLACE ... SELECT` row stream with the
  descriptor-driven key-bearing replacement executor.
- [x] Add C runtime tests for keyed replacement, affected rows, warnings,
  persistence, foreign keys, auto-increment, cleanup, and continued rejection
  of unsupported source forms.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, architecture, performance,
  descriptor authority, file-format safety, cleanup, scope control, and docs.
- [x] Commit and push the completed slice.
