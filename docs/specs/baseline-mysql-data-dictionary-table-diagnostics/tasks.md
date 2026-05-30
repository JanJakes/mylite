# Baseline mysql Data Dictionary Table Diagnostics Tasks

- [x] Record official MySQL 8.4 documentation references for protected data
  dictionary tables and `INFORMATION_SCHEMA` integration.
- [x] Probe MySQL 8.4.9 for direct-read, metadata-statement, and directory
  visibility behavior.
- [x] Specify the MyLite scope as MySQL-shaped diagnostics for hidden
  dictionary names without adding queryable metadata rows.
- [x] Add a MySQL expectation script for the observed target-runtime
  diagnostics.
- [x] Add runtime hidden dictionary-table name classification and diagnostics.
- [x] Add focused C runtime coverage for direct reads, metadata statements,
  and directory absence.
- [x] Update compatibility docs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check` and `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
