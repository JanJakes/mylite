# Baseline sys.sys_config trigger metadata tasks

- [x] Record official MySQL 8.4 documentation references for
  `INFORMATION_SCHEMA.TRIGGERS`, `SHOW TRIGGERS`, and the two sys_config
  trigger objects.
- [x] Probe MySQL 8.4.9 for sys trigger metadata rows and SHOW output.
- [x] Specify the MyLite scope as metadata-only trigger rows without trigger
  execution, trigger DDL, writable sys configuration, or persisted trigger
  descriptors.
- [x] Add a MySQL expectation script for the observed target-runtime metadata.
- [x] Add runtime metadata rows for `INFORMATION_SCHEMA.TRIGGERS`.
- [x] Add runtime `SHOW TRIGGERS` rows for the `sys` schema.
- [x] Add focused C runtime coverage for information-schema and SHOW rows.
- [x] Update compatibility docs and related specs.
- [x] Run focused build and CTest target.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `git diff --check` and `git diff --cached --check`.
- [x] Run `cmake --workflow --preset check`.
- [x] Perform the release-gate review and address findings.
