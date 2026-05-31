# Baseline sys.sys_config table tasks

- [x] Record official MySQL 8.4 documentation references for `sys.sys_config`,
  `SHOW COLUMNS`, `SHOW INDEX`, information-schema metadata, and table status.
- [x] Probe MySQL 8.4.9 for default rows, column metadata, primary-key metadata,
  information-schema rows, and table-status fields.
- [x] Specify the MyLite scope as read-only synthetic default rows plus
  metadata/status/primary-key parity without writable sys configuration,
  triggers, functions, procedures, or views.
- [x] Add a MySQL expectation script for the observed target-runtime metadata.
- [x] Add runtime metadata definitions and synthetic rows for `sys.sys_config`.
- [x] Add focused C runtime coverage for direct reads, selected-schema reads,
  `SHOW` metadata, information-schema metadata, and table-status rows.
- [x] Update compatibility docs and related metadata specs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check` and `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
