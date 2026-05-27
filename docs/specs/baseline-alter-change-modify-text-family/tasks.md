# Baseline ALTER CHANGE/MODIFY TEXT-Family Replacement Tasks

- [x] Confirm MySQL 8.4.9 behavior for the supported success and diagnostic
  cases and store the expectation script.
- [x] Update compatibility documentation for the exact `CHANGE` / `MODIFY`
  text-family replacement subset.
- [x] Extend the replacement-family admission gate for `CHAR` / `VARCHAR` /
  national string / existing text-family sources to supported text-family
  targets.
- [x] Preserve existing descriptor authority, row validation, key validation,
  physical rebuild, and generated SQLite identifier quoting.
- [x] Add MySQL-compatible text-family `NULL` into `NOT NULL` diagnostics for
  this replacement path without changing existing integer behavior.
- [x] Add focused runtime tests for `MODIFY`, `CHANGE`, metadata, indexes,
  diagnostics, persistence, and file-format safety.
- [x] Run focused CTest entries and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the diff for scope, architecture, MySQL evidence, metadata
  authority, file-format safety, cleanup, and test relevance.
- [x] Commit and push the completed feature.
