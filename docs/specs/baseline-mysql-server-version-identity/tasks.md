# Baseline MySQL Server Version Identity Tasks

- [x] Read existing version function and version system-variable specs.
- [x] Verify MySQL 8.4.9 runtime behavior for `VERSION()`, `@@version`,
  `@@global.version`, `@@version_comment`, `@@global.version_comment`, and
  `SHOW VARIABLES` rows.
- [x] Write independently authored feature spec for the SQL-visible server
  version identity override.
- [x] Add internal SQL server identity constants without changing
  `mylite_version()`.
- [x] Route `VERSION()`, `@@version`, and `SHOW VARIABLES` version rows through
  the SQL-visible MySQL 8.4.9 constants.
- [x] Update fast C tests for the SQL-visible values while preserving the
  public version API test.
- [x] Update compatibility docs and superseded version specs.
- [x] Run targeted version/system-variable tests and MySQL expectation scripts.
- [x] Run `cmake --workflow --preset check`.
- [x] Run subagent release-gate review and fix findings.
- [x] Commit atomically and push `main`.
