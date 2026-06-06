# Baseline stored procedure SELECT call tasks

- [x] Record MySQL 8.4.9 expectations for no-argument single-`SELECT`
  procedures, `SHOW CREATE PROCEDURE`, `CALL`, duplicate creation, missing
  routines, and `DROP PROCEDURE IF EXISTS` notes.
- [x] Add parser/AST support for the limited `CREATE PROCEDURE`,
  `DROP PROCEDURE`, `SHOW CREATE PROCEDURE`, and `CALL` subset.
- [x] Add session-local procedure descriptors and cleanup.
- [x] Execute `CALL` through the existing MyLite SELECT path.
- [x] Add parser and runtime regression tests.
- [x] Update compatibility documentation.
