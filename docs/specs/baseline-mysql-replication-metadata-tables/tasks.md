# Baseline mysql replication metadata tables Tasks

- [x] Record official MySQL 8.4 documentation references for the replication
  metadata repository tables.
- [x] Probe MySQL 8.4.9 for row count, column metadata, key metadata,
  information-schema rows, and table status.
- [x] Specify the MyLite scope as read-only empty replication metadata
  repository placeholders.
- [x] Add a MySQL expectation script for the observed target-runtime metadata.
- [x] Add focused C runtime coverage for reads, metadata statements,
  selected-schema reads, and directory/status metadata.
- [x] Implement the supported `mysql` system-table definitions.
- [x] Update compatibility docs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check`.
- [x] Run `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
