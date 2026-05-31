# Baseline sys.schema_auto_increment_columns view tasks

- [x] Record official MySQL 8.4 documentation references for the sys view,
      information-schema metadata, and SHOW CREATE metadata.
- [x] Probe MySQL 8.4.9 for column shape, row values, view definition metadata,
      dependency metadata, and selected-schema behavior.
- [x] Specify the supported synthetic view surface and intentionally unsupported
      empty-table/statistics edge cases.
- [x] Add a MySQL expectation script with runtime-verified outputs.
- [x] Add the synthetic sys view descriptor and row builder.
- [x] Add `INFORMATION_SCHEMA.VIEWS` and `VIEW_TABLE_USAGE` rows.
- [x] Add `SHOW CREATE VIEW` and `SHOW CREATE TABLE` rendering.
- [x] Add focused MyLite runtime coverage.
- [x] Run focused MySQL expectation and MyLite runtime tests.
- [x] Run formatting, diff, workflow, and release-gate checks.
- [x] Update compatibility documentation and prepare the focused slice for
      commit.
