# Baseline INFORMATION_SCHEMA Conditional Table Absence Tasks

- [x] Record official MySQL 8.4 documentation references for conditional
  Enterprise Firewall, Enterprise Thread Pool, and NDB `INFORMATION_SCHEMA`
  tables.
- [x] Probe MySQL 8.4.9 for direct-read, metadata-statement, and directory
  absence behavior.
- [x] Specify the MyLite scope as target-build absence without placeholder
  table definitions.
- [x] Add a MySQL expectation script for the observed target-runtime
  diagnostics.
- [x] Add focused C runtime coverage for direct reads, metadata statements,
  unqualified reads, and directory absence.
- [x] Update compatibility docs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check` and `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
