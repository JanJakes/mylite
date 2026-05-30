# Baseline mysql.innodb_dynamic_metadata diagnostics tasks

- [x] Record official MySQL 8.4 documentation references for the protected
  `mysql.innodb_dynamic_metadata` system table.
- [x] Probe MySQL 8.4.9 for direct-read, metadata-statement, and directory
  visibility behavior.
- [x] Specify the MyLite scope as MySQL-shaped diagnostics without queryable
  metadata rows.
- [x] Extend the MySQL expectation script for the observed target-runtime
  diagnostics.
- [x] Extend runtime hidden system-table name classification and diagnostics.
- [x] Extend focused C runtime coverage for direct reads, metadata statements,
  and directory absence.
- [x] Update compatibility docs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check`.
- [x] Run `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
