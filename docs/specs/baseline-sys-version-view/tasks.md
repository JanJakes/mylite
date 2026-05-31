# Baseline sys.version view tasks

- [x] Record official MySQL 8.4 documentation references for `sys.version`,
  `SHOW COLUMNS`, and information-schema metadata.
- [x] Probe MySQL 8.4.9 for direct rows, column metadata, empty index metadata,
  and table-status fields.
- [x] Specify the MyLite scope as a read-only synthetic row plus
  metadata/status parity without persisted view descriptors or `SHOW CREATE
  VIEW`.
- [x] Add a MySQL expectation script for the observed target-runtime metadata.
- [x] Add runtime metadata definitions and a synthetic row for `sys.version`.
- [x] Add focused C runtime coverage for direct reads, selected-schema reads,
  `SHOW` metadata, information-schema metadata, and empty index/constraint
  rows.
- [x] Update compatibility docs and related specs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check` and `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
