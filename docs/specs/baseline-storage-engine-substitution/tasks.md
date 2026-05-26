# Baseline Storage Engine Substitution Tasks

- [x] Verify MySQL 8.4.9 behavior for unavailable `ENGINE` names under default
  `NO_ENGINE_SUBSTITUTION` and under `SET SESSION sql_mode=''`.
- [x] Specify MyLite's embedded single-engine substitution policy and its
  MyLite-specific treatment of non-InnoDB engine names.
- [x] Add a MySQL expectation artifact for strict errors, loose substitution
  warnings, temporary-table behavior, and `IF NOT EXISTS` warning order.
- [x] Implement SQL-mode-aware engine validation in create-table planning.
- [x] Add runtime coverage for substituted persistent and temporary tables,
  warnings, diagnostics, metadata rendering, persistence, preamble safety, and
  independent handles.
- [x] Update compatibility documentation for the exact limited surface.
- [x] Run focused engine/SQL-mode/create-table tests and the MySQL expectation
  artifact.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the diff, commit atomically, and push `main`.
