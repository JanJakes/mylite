# Baseline Built-In Schema Catalog Tasks

- [x] Verify MySQL 8.4.9 behavior for built-in schema rows, `SHOW DATABASES`
      filters, `INFORMATION_SCHEMA.SCHEMATA`, `USE`, and protected schema DDL.
- [x] Write independently authored feature specification.
- [x] Add MySQL expectation artifact for the verified surface.
- [x] Add synthetic built-in schema helpers in runtime code.
- [x] Merge built-ins into `SHOW DATABASES` / `SHOW SCHEMAS` output without
      persisting catalog descriptors.
- [x] Extend `INFORMATION_SCHEMA.SCHEMATA` synthetic rows.
- [x] Allow `USE` for `mysql`, `performance_schema`, and `sys`, including
      database charset/collation session variable values.
- [x] Protect built-in schema names from supported schema/table writes.
- [x] Update C tests for schema lifecycle and information schema behavior.
- [x] Update compatibility documentation.
- [x] Run focused CTests for schema/information-schema coverage.
- [x] Run the MySQL expectation artifact.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff and address findings.
- [x] Commit and push to `origin/main`.
