# Baseline sys.x$ps_schema_table_statistics_io view tasks

- [x] Record official MySQL 8.4 documentation references for the sys helper
      view surface, Performance Schema file summary source, and SHOW CREATE
      metadata.
- [x] Probe MySQL 8.4.9 for column shape, row presence, view definition
      metadata, table dependency metadata, routine dependency metadata,
      selected-schema behavior, SHOW metadata, and read status behavior.
- [x] Specify descriptor-backed zero-counter placeholder behavior and
      intentionally unsupported live file-summary rows.
- [x] Add a MySQL expectation script with runtime-verified outputs.
- [x] Add the synthetic sys view descriptor and row builder.
- [x] Add `INFORMATION_SCHEMA.VIEWS`, `VIEW_TABLE_USAGE`, and
      `VIEW_ROUTINE_USAGE` rows.
- [x] Add `SHOW CREATE VIEW` and `SHOW CREATE TABLE` rendering.
- [x] Add focused MyLite runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused MySQL expectation and MyLite runtime tests.
- [x] Run formatting, diff, workflow, and release-gate checks.
- [x] Prepare the focused slice for commit.
