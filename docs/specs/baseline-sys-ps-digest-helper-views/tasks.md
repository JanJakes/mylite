# Baseline sys digest helper views tasks

- [x] Record official MySQL 8.4 documentation references for the sys digest
      helper surface and Performance Schema statement-summary source.
- [x] Probe MySQL 8.4.9 for column shape, row presence, view definition
      metadata, table dependency metadata, routine dependency absence,
      selected-schema behavior, SHOW metadata, and read status behavior.
- [x] Specify empty placeholder behavior and intentionally unsupported live
      Performance Schema statement digest behavior.
- [x] Add and rerun a MySQL expectation script with runtime-verified outputs.
- [x] Add the synthetic sys helper view descriptors and empty row builders.
- [x] Add `INFORMATION_SCHEMA.VIEWS` and `VIEW_TABLE_USAGE` rows while keeping
      `VIEW_ROUTINE_USAGE` empty for these helper views.
- [x] Add `SHOW CREATE VIEW` and `SHOW CREATE TABLE` rendering.
- [x] Add focused MyLite runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused MySQL expectation and MyLite runtime tests.
- [x] Run formatting, diff, workflow, and release-gate checks.
- [x] Prepare the focused slice for commit.
