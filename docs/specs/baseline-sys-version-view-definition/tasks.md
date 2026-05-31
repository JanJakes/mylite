# Baseline sys.version view definition metadata tasks

- [x] Record official MySQL 8.4 documentation references for `sys.version`,
      `INFORMATION_SCHEMA.VIEWS`, `SHOW CREATE VIEW`, and `SHOW CREATE TABLE`.
- [x] Probe MySQL 8.4.9 for the `INFORMATION_SCHEMA.VIEWS` row, qualified and
      selected-schema `SHOW CREATE` rows, SHOW status, and empty dependency
      metadata.
- [x] Specify the supported metadata surface and intentionally unsupported
      sys-view behavior.
- [x] Add a MySQL expectation script with runtime-verified outputs.
- [x] Add synthetic `INFORMATION_SCHEMA.VIEWS` metadata for `sys.version`.
- [x] Add synthetic `SHOW CREATE VIEW` and `SHOW CREATE TABLE` rendering for
      `sys.version`.
- [x] Extend MyLite runtime coverage for the new metadata.
- [x] Run focused MySQL expectation and MyLite runtime tests.
- [x] Run formatting, diff, workflow, and release-gate checks.
- [x] Update compatibility documentation and prepare the focused slice for commit.
