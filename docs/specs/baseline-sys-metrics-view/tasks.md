# Baseline sys.metrics view tasks

- [x] Record official MySQL 8.4 documentation references for the sys metrics
      view, status source, and SHOW CREATE metadata.
- [x] Probe MySQL 8.4.9 for column shape, row classes, view definition
      metadata, table dependency metadata, routine dependency absence,
      selected-schema behavior, SHOW metadata, and read status behavior.
- [x] Specify partial status-descriptor behavior and intentionally unsupported
      live Performance Schema, InnoDB, setup-instrument, and system-time rows.
- [x] Add and rerun a MySQL expectation script with runtime-verified outputs.
- [x] Add the synthetic sys view descriptor and status-backed row builder.
- [x] Add `INFORMATION_SCHEMA.VIEWS` and `VIEW_TABLE_USAGE` rows while keeping
      `VIEW_ROUTINE_USAGE` empty for this view.
- [x] Add `SHOW CREATE VIEW` and `SHOW CREATE TABLE` rendering.
- [x] Add focused MyLite runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused MySQL expectation and MyLite runtime tests.
- [x] Run formatting, diff, workflow, and release-gate checks.
- [x] Prepare the focused slice for commit.
