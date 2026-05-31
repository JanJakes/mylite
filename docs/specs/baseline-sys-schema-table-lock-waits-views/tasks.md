# Baseline sys.schema_table_lock_waits views tasks

- [x] Record official MySQL 8.4 documentation references for the sys views,
      information-schema metadata, and SHOW CREATE metadata.
- [x] Probe MySQL 8.4.9 for column shape, empty-row behavior, view definition
      metadata, table dependency metadata, routine dependency metadata, and
      selected-schema behavior.
- [x] Specify the empty embedded placeholder behavior and intentionally
      unsupported Performance Schema lock-wait execution.
- [x] Add a MySQL expectation script with runtime-verified outputs.
- [x] Add synthetic sys view descriptors and empty row builders.
- [x] Add `INFORMATION_SCHEMA.VIEWS`, `VIEW_TABLE_USAGE`, and
      `VIEW_ROUTINE_USAGE` rows.
- [x] Add `SHOW CREATE VIEW` and `SHOW CREATE TABLE` rendering.
- [x] Add focused MyLite runtime coverage.
- [x] Run focused MySQL expectation and MyLite runtime tests.
- [x] Run formatting, diff, workflow, and release-gate checks.
- [x] Update compatibility documentation and prepare the focused slice for
      commit.
