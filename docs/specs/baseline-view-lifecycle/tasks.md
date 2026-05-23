# Baseline View Lifecycle Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, catalog, parser, runtime, and
  existing view metadata specs.
- [x] Review official MySQL 8.4 documentation for view DDL, SHOW, and
  information-schema behavior.
- [x] Probe MySQL 8.4.9 runtime behavior for create/drop/show/metadata
  expectations and diagnostics.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Extend lexer/parser/AST support for `CREATE VIEW`, `DROP VIEW`, and
  `SHOW CREATE VIEW`.
- [x] Add durable view catalog kind, schema migration, descriptor storage, and
  cleanup.
- [x] Implement descriptor-driven create/drop/show view lifecycle runtime.
- [x] Wire view rows into `SHOW TABLES`, `SHOW TABLE STATUS`, `SHOW COLUMNS`,
  `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE`,
  `INFORMATION_SCHEMA.TABLES`, and `INFORMATION_SCHEMA.COLUMNS`.
- [x] Add focused parser and runtime C tests.
- [x] Update compatibility documentation.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.
