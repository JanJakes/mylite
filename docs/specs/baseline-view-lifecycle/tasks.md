# Baseline View Lifecycle Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, catalog, parser, runtime, and
  existing view metadata specs.
- [x] Review official MySQL 8.4 documentation for view DDL, SHOW, and
  information-schema behavior.
- [x] Probe MySQL 8.4.9 runtime behavior for create/drop/show/metadata
  expectations and diagnostics.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [ ] Extend lexer/parser/AST support for `CREATE VIEW`, `DROP VIEW`, and
  `SHOW CREATE VIEW`.
- [ ] Add durable view catalog kind, schema migration, descriptor storage, and
  cleanup.
- [ ] Implement descriptor-driven create/drop/show view lifecycle runtime.
- [ ] Wire view rows into `SHOW TABLES`, `SHOW TABLE STATUS`, `SHOW COLUMNS`,
  `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE`,
  `INFORMATION_SCHEMA.TABLES`, and `INFORMATION_SCHEMA.COLUMNS`.
- [ ] Add focused parser and runtime C tests.
- [ ] Update compatibility documentation.
- [ ] Run focused build/tests and MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend findings, commit, and push to remote
  `main`.
