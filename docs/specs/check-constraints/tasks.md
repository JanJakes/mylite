# CHECK constraints task list

- [x] Record inline and table-level `CREATE TABLE ... CHECK` metadata in
      `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` and
      `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`.
- [x] Verify MySQL 8.4.9 runtime behavior for enforced, not-enforced, `NULL`,
      `IGNORE`, ODKU, `UPDATE`, and `REPLACE` CHECK paths.
- [x] Enforce cataloged `CREATE TABLE` CHECK constraints for supported
      `INSERT`, `INSERT ... SET`, ODKU, `REPLACE`, single-table `UPDATE`, and
      joined `UPDATE` rows.
- [x] Demote CHECK violations to warning 3819 and skip rows for covered
      `INSERT IGNORE` and `UPDATE IGNORE` paths.
- [x] Enforce CHECK constraints for temporary tables through the temporary
      CHECK catalog.
- [x] Implement `ALTER TABLE ... ADD CHECK`, including generated names,
      existing-row validation, `NOT ENFORCED`, statement atomicity, and
      MySQL-compatible duplicate-name diagnostics.
- [x] Implement `ALTER TABLE ... DROP CHECK` and `DROP CONSTRAINT` metadata
      cleanup with MySQL-compatible missing-name diagnostics.
- [x] Implement `ALTER TABLE ... ALTER CHECK ... ENFORCED/NOT ENFORCED`,
      including existing-row validation when enabling enforcement.
- [ ] Support CHECK actions mixed with column/index/table-option ALTER
      statements.
- [ ] Validate CHECK expressions at DDL time for deterministic, row-local
      MySQL rules and reject subqueries, variables, unsupported functions,
      references to other tables, and invalid `AUTO_INCREMENT` references.
- [ ] Render CHECK clauses in `SHOW CREATE TABLE` with MySQL-compatible
      formatting and `/*!80016 NOT ENFORCED */`.
- [ ] Enforce schema-level CHECK constraint name uniqueness for `CREATE TABLE`.
