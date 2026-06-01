# Baseline sys.host_summary_by_file_io_type views tasks

- [x] Record official MySQL 8.4 documentation references for the sys host
      summary by file I/O type view surface, Performance Schema wait-summary
      source, and SHOW CREATE metadata.
- [x] Probe MySQL 8.4.9 for column shape, row presence, view definition
      metadata, table dependency metadata, routine dependency absence,
      selected-schema behavior, SHOW metadata, and read status behavior.
- [x] Specify empty placeholder behavior and intentionally unsupported live
      host/event wait-summary and formatting-function behavior.
- [x] Add a MySQL expectation script with runtime-verified outputs.
- [x] Add the synthetic sys view descriptors and empty row builders.
- [x] Add `INFORMATION_SCHEMA.VIEWS` and `VIEW_TABLE_USAGE` rows while keeping
      `VIEW_ROUTINE_USAGE` empty for these views.
- [x] Add `SHOW CREATE VIEW` and `SHOW CREATE TABLE` rendering.
- [x] Add focused MyLite runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused MySQL expectation and MyLite runtime tests.
- [x] Run formatting, diff, workflow, and release-gate checks.
- [x] Prepare the focused slice for commit.
