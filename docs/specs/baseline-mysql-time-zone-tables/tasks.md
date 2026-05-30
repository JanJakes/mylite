# Baseline mysql Time Zone Tables Tasks

- [x] Record official MySQL 8.4 documentation references for time-zone system
  tables, `SHOW COLUMNS`, `SHOW INDEX`, information-schema metadata, and
  table status.
- [x] Probe MySQL 8.4.9 for direct row counts, column metadata, primary-key
  metadata, information-schema rows, and table-status fields.
- [x] Specify the MyLite scope as metadata/status/primary-key parity with
  empty read-only placeholder rows and explicit zoneinfo-loading
  incompatibility.
- [x] Add a MySQL expectation script for the observed target-runtime metadata.
- [x] Add runtime metadata definitions for the five `mysql.time_zone*` tables.
- [x] Add focused C runtime coverage for direct empty placeholders, `SHOW`
  metadata, information-schema metadata, and table-status rows.
- [x] Update compatibility docs and related metadata specs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check` and `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
