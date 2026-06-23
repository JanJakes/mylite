# Baseline sys.session views tasks

- [x] Record official MySQL 8.4 documentation references for session and
      session SSL views.
- [x] Probe MySQL 8.4.9 for session and SSL-status column shape, view metadata,
      dependencies, `SHOW CREATE`, current rows, no-SSL values, and read
      status.
- [x] Add and rerun a MySQL expectation script with runtime-verified outputs.
- [x] Add synthetic sys descriptors and built-in view definitions.
- [x] Add `INFORMATION_SCHEMA.VIEWS`, `VIEW_TABLE_USAGE`, and empty
      `VIEW_ROUTINE_USAGE` metadata.
- [x] Add processlist-backed row building for session views and SSL-status
      placeholders.
- [x] Add focused MyLite runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused MySQL expectation and MyLite runtime tests.
