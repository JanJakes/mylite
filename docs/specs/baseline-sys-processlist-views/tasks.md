# Baseline sys.processlist views tasks

- [x] Record official MySQL 8.4 documentation references for sys processlist
      views and underlying Performance Schema sources.
- [x] Probe MySQL 8.4.9 for formatted/raw column shape, view metadata,
      dependencies, routine usage, `SHOW CREATE`, current rows, and read
      status.
- [x] Specify partial MyLite processlist-registry behavior and intentionally
      unsupported Performance Schema timing, wait, memory, and connection
      attribute values.
- [x] Add and rerun a MySQL expectation script with runtime-verified outputs.
- [x] Add synthetic sys descriptors and built-in view definitions.
- [x] Add `INFORMATION_SCHEMA.VIEWS`, `VIEW_TABLE_USAGE`, and
      `VIEW_ROUTINE_USAGE` metadata.
- [x] Add processlist-backed row building for both views.
- [x] Add focused MyLite runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused MySQL expectation and MyLite runtime tests.
