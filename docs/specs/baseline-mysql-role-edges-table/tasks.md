# Baseline mysql.role_edges table tasks

- [x] Record official MySQL 8.4 documentation references for grant tables and
  the `mysql` system schema.
- [x] Probe MySQL 8.4.9 for direct row counts, metadata, indexes,
  constraints, and table status.
- [x] Specify the MyLite scope as an empty read-only metadata placeholder
  without role graph storage or enforcement.
- [x] Add a MySQL expectation script for the observed target-runtime behavior.
- [x] Add focused C runtime coverage for direct reads, metadata statements,
  selected-schema reads, constraints, and table status.
- [x] Implement the `mysql.role_edges` table definition and index metadata.
- [x] Update compatibility docs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check`.
- [x] Run `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
