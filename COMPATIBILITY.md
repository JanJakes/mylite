# MyLite ↔ MySQL 8.4.9 compatibility

This document is the initial compatibility target inventory for MyLite against MySQL 8 LTS, currently MySQL 8.4.9. It replaces the seed matrix with a MyLite-specific catalog of SQL grammar, runtime semantics, metadata, protocol, and operational surfaces that must be tracked as implementation progresses.

## Legend

| Mark | Meaning |
| :-: | --- |
| ✅ | Supported with MySQL 8.4.9-compatible behavior and runtime comparison tests for relevant results, errors, warnings, metadata, affected rows, side effects, and protocol details. |
| 🟡 | Implemented with documented compatibility gaps, reduced fidelity, or MyLite-specific behavior that is covered by tests. |
| ⚪ | Accepted at parse or API level and intentionally handled as an embedded-compatible no-op, placeholder, warning, or diagnostic. |
| ❌ | Not implemented, rejected, or not yet MySQL-runtime verified in MyLite. |

## Source Inventory

- [MySQL 8.4 Reference Manual](https://dev.mysql.com/doc/refman/8.4/en/)
- [MySQL 8.4.9 release notes](https://dev.mysql.com/doc/relnotes/mysql/8.4/en/news-8-4-9.html)
- [What Is New in MySQL 8.4 since MySQL 8.0](https://dev.mysql.com/doc/refman/8.4/en/mysql-nutshell.html)
- [MySQL 8.4 SQL statement manual](https://dev.mysql.com/doc/refman/8.4/en/sql-statements.html)
- [MySQL 8.4 built-in function and operator reference](https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html)
- [MySQL 8.4 data type manual](https://dev.mysql.com/doc/refman/8.4/en/data-types.html)
- [MySQL 8.4 `INFORMATION_SCHEMA` table reference](https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html)
- [MySQL 8.4 Performance Schema table descriptions](https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-descriptions.html)
- [MySQL 8.4 Performance Schema table reference](https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-reference.html)
- [MySQL 8.4 `sys` schema object reference](https://dev.mysql.com/doc/refman/8.4/en/sys-schema-reference.html)
- [MySQL 8.4 `mysql` system schema reference](https://dev.mysql.com/doc/refman/8.4/en/system-schema.html)
- [MySQL 8.4 server system variable reference](https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html)
- [MySQL 8.4 server status variable reference](https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html)
- [MySQL 8.4.9 parser grammar (`sql/sql_yacc.yy`)](https://github.com/mysql/mysql-server/blob/mysql-8.4.9/sql/sql_yacc.yy)
- [MySQL 8.4.9 protocol command enum (`include/my_command.h`)](https://github.com/mysql/mysql-server/blob/mysql-8.4.9/include/my_command.h)
- [MySQL 8.4.9 `sys` schema source (`scripts/sys_schema`)](https://github.com/mysql/mysql-server/tree/mysql-8.4.9/scripts/sys_schema)

## Maintenance Rules

- Keep this file as an implementation contract, not a marketing scorecard.
- Split rows when syntax, behavior, metadata, warnings, or side effects need independent tests.
- Document deliberate embedded-design incompatibilities explicitly instead of silently dropping them.
- Keep `COMPATIBILITY.md`, feature guides, design notes, and MySQL-runtime tests in sync.
- Prefer MySQL 8.4.9 behavior over convenience, SQLite defaults, or older MySQL/MariaDB behavior.

## 1. SQL Statement Surface

The parser should eventually recognize the full MySQL grammar. Unsupported embedded-server features may still be accepted with a MySQL-compatible diagnostic, warning, or placeholder when that is safer for applications than a syntax error.

### 1.1 Data Definition Statements

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `ALTER DATABASE` / `ALTER SCHEMA` | ❌ | Database default character set, collation, encryption, and read-only options. |  |
| `ALTER EVENT` | ❌ | Event scheduler metadata and body changes. |  |
| `ALTER FUNCTION` | ❌ | Stored-function metadata characteristics. |  |
| `ALTER INSTANCE` | ❌ | Instance reload, TLS, keyring, and master-key operations with embedded-compatible behavior. |  |
| `ALTER LOGFILE GROUP` | ❌ | NDB logfile group syntax and diagnostics. |  |
| `ALTER PROCEDURE` | ❌ | Stored-procedure metadata characteristics. |  |
| `ALTER SERVER` | ❌ | Foreign server metadata changes. |  |
| `ALTER TABLE` | ❌ | Full table rebuild/in-place/instant surface; see section 3.2. |  |
| `ALTER TABLESPACE` | ❌ | General tablespace alterations and diagnostics. |  |
| `ALTER UNDO TABLESPACE` | ❌ | Undo tablespace syntax from the MySQL parser. |  |
| `ALTER VIEW` | ❌ | View replacement while preserving MySQL metadata and security semantics. |  |
| `CREATE DATABASE` / `CREATE SCHEMA` | ❌ | Database creation syntax, defaults, warnings, and single-file mapping. |  |
| `CREATE EVENT` | ❌ | Scheduled event definition, body, definer, comments, and scheduler metadata. |  |
| `CREATE FUNCTION` (stored) | ❌ | Stored-function definition, determinism, SQL data access, security, and body semantics. |  |
| `CREATE FUNCTION` (loadable) | ❌ | Loadable-function registration syntax with embedded-compatible diagnostics. |  |
| `CREATE INDEX` | ❌ | Standalone index creation over MySQL index types and attributes. |  |
| `CREATE LOGFILE GROUP` | ❌ | NDB logfile group syntax and diagnostics. |  |
| `CREATE PROCEDURE` | ❌ | Stored procedure parameters, body, characteristics, and diagnostics. |  |
| `CREATE SERVER` | ❌ | Foreign server metadata syntax. |  |
| `CREATE SPATIAL REFERENCE SYSTEM` | ❌ | Spatial reference system catalog DDL. |  |
| `CREATE TABLE` | ❌ | Column definitions, constraints, indexes, table options, generated columns, and partitions; see section 3.1. |  |
| `CREATE TEMPORARY TABLE` | ❌ | Session-scoped table lifecycle and name shadowing. |  |
| `CREATE TABLE ... LIKE` | ❌ | Exact metadata cloning rules. |  |
| `CREATE TABLE ... SELECT` | ❌ | CTAS type inference, default handling, indexes, and atomicity. |  |
| `CREATE TABLESPACE` | ❌ | General and NDB tablespace syntax and diagnostics. |  |
| `CREATE UNDO TABLESPACE` | ❌ | Undo tablespace syntax present in the MySQL 8.4 parser source. |  |
| `CREATE TRIGGER` | ❌ | Trigger timing, event, ordering, body, definer, and metadata. |  |
| `CREATE VIEW` | ❌ | View column names, algorithms, security, check options, and metadata. |  |
| `DROP DATABASE` / `DROP SCHEMA` | ❌ | Schema removal, metadata cleanup, warnings, and embedded single-file constraints. |  |
| `DROP EVENT` | ❌ | Event metadata deletion. |  |
| `DROP FUNCTION` (stored) | ❌ | Stored-function deletion and routine metadata cleanup. |  |
| `DROP FUNCTION` (loadable) | ❌ | Loadable-function deregistration syntax. |  |
| `DROP INDEX` | ❌ | Standalone index removal semantics. |  |
| `DROP LOGFILE GROUP` | ❌ | NDB logfile group syntax and diagnostics. |  |
| `DROP PROCEDURE` | ❌ | Stored-procedure deletion and metadata cleanup. |  |
| `DROP SERVER` | ❌ | Foreign server metadata deletion. |  |
| `DROP SPATIAL REFERENCE SYSTEM` | ❌ | Spatial reference system deletion and dependency checks. |  |
| `DROP TABLE` | ❌ | Multi-table drop, temporary tables, foreign-key checks, and warnings. |  |
| `DROP TABLESPACE` | ❌ | Tablespace deletion syntax and diagnostics. |  |
| `DROP UNDO TABLESPACE` | ❌ | Undo tablespace deletion syntax present in the MySQL 8.4 parser source. |  |
| `DROP TRIGGER` | ❌ | Trigger deletion and metadata cleanup. |  |
| `DROP VIEW` | ❌ | Multi-view drop and warnings. |  |
| `RENAME TABLE` | ❌ | Atomic multi-table rename semantics. |  |
| `TRUNCATE TABLE` | ❌ | DDL-like truncate, auto-increment reset, implicit commit, and foreign-key restrictions. |  |
| Atomic DDL | ❌ | Atomicity and crash-safety expectations for MySQL DDL equivalents. |  |
| Implicit commit boundaries | ❌ | Statements that cause implicit commits before and/or after execution. |  |

### 1.2 Data Manipulation Statements

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `CALL` | ❌ | Procedure invocation, result sets, OUT/INOUT parameters, and diagnostics. |  |
| `DELETE` (single-table) | ❌ | Single-table delete with aliases, partitions, ORDER BY, LIMIT, LOW_PRIORITY, QUICK, and IGNORE. |  |
| `DELETE` (multi-table) | ❌ | Multi-table delete forms using FROM and USING, join semantics, and affected rows. |  |
| `DO` | ❌ | Expression execution with warning and error semantics. |  |
| `HANDLER` | ❌ | HANDLER OPEN, READ, and CLOSE cursor-like table access. |  |
| `IMPORT TABLE` | ❌ | Transportable tablespace import syntax and diagnostics. |  |
| `INSERT ... VALUES` | ❌ | Multi-row values, defaults, generated columns, warnings, affected rows, and insert ids. |  |
| `INSERT ... SET` | ❌ | MySQL SET-form insert semantics. |  |
| `INSERT ... SELECT` | ❌ | Insert from query expression with locking, defaults, and metadata inference. |  |
| `INSERT ... ON DUPLICATE KEY UPDATE` | ❌ | Conflict target resolution, VALUES()/row aliases, affected rows, and warnings. |  |
| `INSERT IGNORE` | ❌ | Duplicate, conversion, and constraint warning demotion rules. |  |
| `INSERT DELAYED` | ❌ | Deprecated delayed insert syntax and MySQL-compatible diagnostics. |  |
| `INSERT LOW_PRIORITY` / `HIGH_PRIORITY` | ❌ | Priority modifiers and embedded-compatible treatment. |  |
| `LOAD DATA INFILE` | ❌ | Server-side text import syntax, field/line options, user variables, SET clause, warnings, and security restrictions. |  |
| `LOAD DATA LOCAL INFILE` | ❌ | Client-side LOCAL INFILE request flow, security controls, warnings, and protocol interaction. |  |
| `LOAD XML INFILE` | ❌ | Server-side XML import syntax, row matching, namespaces, SET clause, warnings, and security restrictions. |  |
| `LOAD XML LOCAL INFILE` | ❌ | Client-side XML import request behavior and embedded-compatible diagnostics. |  |
| `REPLACE ... VALUES` | ❌ | Delete-then-insert semantics, cascades, triggers, affected rows, and auto-increment behavior. |  |
| `REPLACE ... SET` | ❌ | SET-form replace semantics. |  |
| `REPLACE ... SELECT` | ❌ | Replace from query expression semantics. |  |
| `REPLACE LOW_PRIORITY` / `DELAYED` | ❌ | Priority and deprecated delayed modifiers for REPLACE. |  |
| `SELECT` | ❌ | Full query expression surface; see section 2. |  |
| `SELECT ... INTO var_list` | ❌ | User/local variable assignment semantics. |  |
| `SELECT ... INTO OUTFILE` | ❌ | File export syntax and embedded-compatible diagnostics. |  |
| `SELECT ... INTO DUMPFILE` | ❌ | Binary file export syntax and embedded-compatible diagnostics. |  |
| `TABLE` | ❌ | Table-value statement syntax and ordering/limit behavior. |  |
| `UPDATE` (single-table) | ❌ | Assignment order, generated columns, ORDER BY, LIMIT, LOW_PRIORITY, and IGNORE. |  |
| `UPDATE` (multi-table) | ❌ | Joined update semantics, assignment evaluation, and affected rows. |  |
| `VALUES` | ❌ | Standalone values statement and row constructor behavior. |  |

### 1.3 Transactional, Locking, Replication, Prepared, and Compound Statements

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `START TRANSACTION` | ❌ | Transaction start modifiers including READ WRITE, READ ONLY, and WITH CONSISTENT SNAPSHOT. |  |
| `BEGIN` / `BEGIN WORK` | ❌ | Transaction begin statement distinct from compound BEGIN ... END. |  |
| `COMMIT` | ❌ | AND CHAIN, AND NO CHAIN, RELEASE, NO RELEASE, completion_type, and diagnostics. |  |
| `ROLLBACK` | ❌ | AND CHAIN, AND NO CHAIN, RELEASE, NO RELEASE, completion_type, and diagnostics. |  |
| `SAVEPOINT` | ❌ | Nested savepoint creation and replacement semantics. |  |
| `ROLLBACK TO SAVEPOINT` | ❌ | Partial rollback semantics and errors. |  |
| `RELEASE SAVEPOINT` | ❌ | Savepoint release semantics and errors. |  |
| `SET TRANSACTION` | ❌ | Isolation level and access mode at global/session/next-transaction scope. |  |
| `LOCK INSTANCE FOR BACKUP` | ❌ | Backup lock syntax and embedded-compatible behavior. |  |
| `UNLOCK INSTANCE` | ❌ | Backup lock release syntax. |  |
| `LOCK TABLES` | ❌ | READ, READ LOCAL, WRITE, LOW_PRIORITY WRITE, aliases, and implicit commit behavior. |  |
| `UNLOCK TABLES` | ❌ | Table lock release and transaction interaction. |  |
| `XA START` | ❌ | XA transaction branch start. |  |
| `XA END` | ❌ | XA transaction branch end. |  |
| `XA PREPARE` | ❌ | XA prepare phase. |  |
| `XA COMMIT` | ❌ | XA one-phase and two-phase commit. |  |
| `XA ROLLBACK` | ❌ | XA rollback. |  |
| `XA RECOVER` | ❌ | XA recovery result-set metadata. |  |
| `BINLOG` | ❌ | Base64 binary log event statement syntax and embedded-compatible diagnostics. |  |
| `PURGE BINARY LOGS` | ❌ | Binary log purge syntax. |  |
| `RESET BINARY LOGS AND GTIDS` | ❌ | Binary log and GTID reset syntax. |  |
| `SET sql_log_bin` | ❌ | Session binary logging toggle and privilege semantics. |  |
| `CHANGE REPLICATION FILTER` | ❌ | Replication filter syntax and diagnostics. |  |
| `CHANGE REPLICATION SOURCE TO` | ❌ | Source connection/channel options and diagnostics. |  |
| `RESET REPLICA` | ❌ | Replica metadata reset syntax. |  |
| `START REPLICA` | ❌ | Replica start syntax, channels, threads, and until conditions. |  |
| `STOP REPLICA` | ❌ | Replica stop syntax and channel handling. |  |
| `START GROUP_REPLICATION` | ❌ | Group Replication start syntax and user credentials. |  |
| `STOP GROUP_REPLICATION` | ❌ | Group Replication stop syntax. |  |
| `PREPARE` | ❌ | Prepare from literal or user variable, parameter marker rules, and errors. |  |
| `EXECUTE` | ❌ | Prepared-statement execution with USING variables and result metadata. |  |
| `DEALLOCATE PREPARE` / `DROP PREPARE` | ❌ | Prepared statement cleanup. |  |
| `BEGIN ... END` | ❌ | Compound statement block scope for stored programs and events. |  |
| Statement labels | ❌ | Label declaration, LEAVE/ITERATE binding, and duplicate-label diagnostics. |  |
| `DECLARE` local variables | ❌ | Stored-program local variable declarations, defaults, and scope. |  |
| `DECLARE ... CONDITION` | ❌ | Named condition declarations. |  |
| `DECLARE ... CURSOR` | ❌ | Cursor declaration over SELECT statements. |  |
| `DECLARE ... HANDLER` | ❌ | CONTINUE/EXIT handler declarations for SQLSTATE, errors, warnings, and NOT FOUND. |  |
| `CASE` statement | ❌ | Stored-program CASE statement semantics. |  |
| `IF` statement | ❌ | Stored-program IF/ELSEIF/ELSE semantics. |  |
| `LOOP` | ❌ | Stored-program LOOP semantics. |  |
| `REPEAT` | ❌ | Stored-program REPEAT UNTIL semantics. |  |
| `WHILE` | ❌ | Stored-program WHILE semantics. |  |
| `ITERATE` | ❌ | Loop iteration transfer. |  |
| `LEAVE` | ❌ | Block/loop exit transfer. |  |
| `RETURN` | ❌ | Stored-function return semantics. |  |
| `OPEN` cursor | ❌ | Cursor open lifecycle. |  |
| `FETCH` cursor | ❌ | Cursor fetch into variables and NOT FOUND handling. |  |
| `CLOSE` cursor | ❌ | Cursor close lifecycle. |  |
| `GET DIAGNOSTICS` | ❌ | Current and stacked diagnostics retrieval. |  |
| `SIGNAL` | ❌ | User-raised SQLSTATE and condition item semantics. |  |
| `RESIGNAL` | ❌ | Handler rethrow and diagnostics mutation. |  |

### 1.4 Account, Resource, Plugin, Maintenance, SHOW, and Utility Statements

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `ALTER USER` | ❌ | Authentication plugins, passwords, MFA, TLS, resources, lock/expire, comments, attributes, and default roles. |  |
| `CREATE USER` | ❌ | User creation syntax, IF NOT EXISTS, auth factors, TLS, resources, password options, and comments. |  |
| `CREATE ROLE` | ❌ | Role creation syntax and metadata. |  |
| `DROP USER` | ❌ | User deletion syntax and privilege cleanup. |  |
| `DROP ROLE` | ❌ | Role deletion syntax and grant cleanup. |  |
| `GRANT` | ❌ | Privilege and role grants, WITH GRANT OPTION, PROXY, dynamic privileges, and partial revoke semantics. |  |
| `RENAME USER` | ❌ | User rename syntax and privilege metadata. |  |
| `REVOKE` | ❌ | Privilege and role revocation semantics. |  |
| `SET DEFAULT ROLE` | ❌ | Default role assignment. |  |
| `SET PASSWORD` | ❌ | Password assignment semantics. |  |
| `SET ROLE` | ❌ | Active-role selection. |  |
| `CREATE RESOURCE GROUP` | ❌ | Thread resource group creation syntax. |  |
| `ALTER RESOURCE GROUP` | ❌ | Resource group modification syntax. |  |
| `DROP RESOURCE GROUP` | ❌ | Resource group deletion syntax. |  |
| `SET RESOURCE GROUP` | ❌ | Thread assignment to resource groups. |  |
| `ANALYZE TABLE` | ❌ | Statistics refresh, histogram update/drop, validation, and result-set metadata. |  |
| `CHECK TABLE` | ❌ | Table consistency checks and result-set metadata. |  |
| `CHECKSUM TABLE` | ❌ | Table checksum syntax and result-set metadata. |  |
| `OPTIMIZE TABLE` | ❌ | Table optimization syntax and result-set metadata. |  |
| `REPAIR TABLE` | ❌ | Repair syntax and result-set metadata. |  |
| `INSTALL COMPONENT` | ❌ | Component installation syntax and diagnostics. |  |
| `UNINSTALL COMPONENT` | ❌ | Component uninstallation syntax and diagnostics. |  |
| `INSTALL PLUGIN` | ❌ | Plugin installation syntax and diagnostics. |  |
| `UNINSTALL PLUGIN` | ❌ | Plugin uninstallation syntax and diagnostics. |  |
| `CLONE` | ❌ | Local and remote clone syntax and diagnostics. |  |
| `SET` | ❌ | Variable assignment, user variables, system variables, persisted variables, names, charset, and transaction forms. |  |
| `SET CHARACTER SET` | ❌ | Connection character-set shorthand semantics. |  |
| `SET NAMES` | ❌ | Connection character set and collation semantics. |  |
| `CACHE INDEX` | ❌ | MyISAM key cache assignment syntax. |  |
| `FLUSH` | ❌ | FLUSH variants for logs, tables, privileges, status, hosts, optimizer costs, and user resources. |  |
| `KILL` | ❌ | Connection/query kill syntax and diagnostics. |  |
| `LOAD INDEX INTO CACHE` | ❌ | MyISAM index preload syntax. |  |
| `RESET` | ❌ | RESET variants for source/replica/persist-style operations exposed by MySQL 8.4. |  |
| `RESET PERSIST` | ❌ | Persisted system variable reset syntax. |  |
| `RESTART` | ❌ | Server restart syntax and embedded-compatible diagnostics. |  |
| `SHUTDOWN` | ❌ | Server shutdown syntax and embedded-compatible diagnostics. |  |
| `DESCRIBE` / `DESC` | ❌ | Table, column, and statement description semantics. |  |
| `EXPLAIN` | ❌ | Explain SELECT/TABLE/INSERT/UPDATE/DELETE, formats, ANALYZE, and FOR CONNECTION. |  |
| `HELP` | ❌ | Server help lookup result-set semantics. |  |
| `USE` | ❌ | Default schema selection in the embedded single-file model. |  |

### 1.5 SHOW Statements

| SHOW statement | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `SHOW BINARY LOG STATUS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW BINARY LOGS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW BINLOG EVENTS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CHARACTER SET` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COLLATION` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COLUMNS` / `SHOW FIELDS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COUNT(*) ERRORS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COUNT(*) WARNINGS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE DATABASE` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE EVENT` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE FUNCTION` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE PROCEDURE` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE TABLE` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE TRIGGER` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE USER` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE VIEW` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW DATABASES` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ENGINE` | ❌ | Generic SHOW ENGINE syntax, subcommand dispatch, result-set shape, and engine-specific diagnostics. |  |
| `SHOW ENGINE LOGS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ENGINE MUTEX` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ENGINE STATUS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ENGINES` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ERRORS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW EVENTS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW FUNCTION CODE` | ❌ | Debug-build-only stored-function instruction listing, privileges, result-set shape, and MySQL-compatible diagnostics when unavailable. | Conditional surface; available only for debug-capable builds. |
| `SHOW FUNCTION STATUS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW GRANTS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW MASTER STATUS` | ❌ | No longer supported in MySQL 8.4; match parser/diagnostic behavior rather than returning binary-log status rows. | Replacement statement is `SHOW BINARY LOG STATUS`. |
| `SHOW OPEN TABLES` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PARSE_TREE` | ❌ | JSON parse-tree result for the MySQL 8.4.9 grammar when enabled; syntax error when built without `WITH_SHOW_PARSE_TREE`. | Conditional debug/development surface. |
| `SHOW PLUGINS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PRIVILEGES` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROCEDURE CODE` | ❌ | Debug-build-only stored-procedure instruction listing, privileges, result-set shape, and MySQL-compatible diagnostics when unavailable. | Conditional surface; available only for debug-capable builds. |
| `SHOW PROCEDURE STATUS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROCESSLIST` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROFILE` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROFILES` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW RELAYLOG EVENTS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW REPLICA STATUS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW REPLICAS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW STATUS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW TABLE STATUS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW TABLES` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW TRIGGERS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW VARIABLES` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW WARNINGS` | ❌ | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |

## 2. Query Expressions and SELECT Semantics

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| Query expression grammar | ❌ | Parenthesized query expressions, query terms, and query primary rules. |  |
| `WITH` common table expressions | ❌ | Non-recursive CTEs, column lists, name shadowing, and scope. |  |
| `WITH RECURSIVE` | ❌ | Recursive CTE execution, cycle behavior, limits, and type inference. |  |
| Projection list | ❌ | Expression aliases, wildcard expansion, qualified wildcards, duplicate names, and metadata. |  |
| Table references | ❌ | Base tables, aliases, schema qualifiers, derived tables, table functions, and parentheses. |  |
| Inner joins | ❌ | JOIN, INNER JOIN, CROSS JOIN, comma join, ON, and USING. |  |
| Outer joins | ❌ | LEFT/RIGHT OUTER JOIN null-extension and predicate placement. |  |
| Natural joins | ❌ | NATURAL INNER/LEFT/RIGHT JOIN column matching and metadata. |  |
| `STRAIGHT_JOIN` | ❌ | Join-order forcing syntax and optimizer interaction. |  |
| Lateral derived tables | ❌ | LATERAL derived table correlation rules. |  |
| `WHERE` | ❌ | Predicate semantics, type conversion, three-valued logic, and short-circuit-sensitive warnings. |  |
| `GROUP BY` | ❌ | Grouping expression semantics, ordinals, aliases, functional-dependence handling, and ONLY_FULL_GROUP_BY. |  |
| `WITH ROLLUP` | ❌ | Super-aggregate rows and GROUPING() behavior. |  |
| `HAVING` | ❌ | Post-group predicate semantics and alias resolution. |  |
| Window definitions | ❌ | WINDOW clause, named windows, inheritance, partitioning, ordering, frames, and MySQL restrictions. |  |
| `ORDER BY` | ❌ | Expression, alias, ordinal, collation, ASC/DESC, and filesort metadata behavior. |  |
| `LIMIT` / `OFFSET` | ❌ | Row limiting, prepared markers, integer conversion, and error cases. |  |
| `DISTINCT` / `DISTINCTROW` | ❌ | Duplicate elimination semantics and metadata. |  |
| `UNION` | ❌ | ALL/DISTINCT semantics, column names/types, ordering, limits, and parentheses. |  |
| `INTERSECT` | ❌ | MySQL 8.4 set operator semantics. |  |
| `EXCEPT` | ❌ | MySQL 8.4 set operator semantics. |  |
| Scalar subqueries | ❌ | Single-value cardinality, NULL behavior, correlation, and errors. |  |
| Row subqueries | ❌ | Row constructors and multi-column comparison semantics. |  |
| `EXISTS` subqueries | ❌ | Existence semantics and correlation behavior. |  |
| `IN` subqueries | ❌ | NULL-aware membership semantics and type conversion. |  |
| `ANY` / `SOME` / `ALL` subqueries | ❌ | Quantified comparison semantics. |  |
| Derived table materialization/merge | ❌ | Optimizer-visible semantics and metadata results. |  |
| Index hints | ❌ | USE/FORCE/IGNORE INDEX with FOR JOIN/ORDER BY/GROUP BY scopes. |  |
| Optimizer hints | ❌ | Comment-hint grammar and ignored/accepted hint diagnostics. |  |
| `PARTITION` selection | ❌ | Explicit partition selection syntax and errors. |  |
| Locking clauses | ❌ | FOR UPDATE, FOR SHARE, OF table list, NOWAIT, and SKIP LOCKED. |  |
| SELECT modifiers | ❌ | ALL, HIGH_PRIORITY, SQL_SMALL_RESULT, SQL_BIG_RESULT, SQL_BUFFER_RESULT, SQL_CALC_FOUND_ROWS, and STRAIGHT_JOIN. |  |
| Expression metadata | ❌ | Column type, length, decimals, flags, charset, collation, nullability, and origin metadata. |  |

## 3. DDL Detail Surface

### 3.1 CREATE TABLE, Index, and Constraint Features

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| Column definition grammar | ❌ | Type, nullability, defaults, visibility, comments, storage, format, references, and constraints. |  |
| Silent column specification changes | ❌ | MySQL automatic rewrites of column specifications and SHOW CREATE output. |  |
| Default expressions | ❌ | Literal and expression defaults, CURRENT_TIMESTAMP variants, and error cases. |  |
| Generated columns | ❌ | Virtual/stored generated columns, dependencies, indexes, and metadata. |  |
| Invisible columns | ❌ | Implicit column lists, SELECT * behavior, and metadata flags. |  |
| Generated invisible primary keys | ❌ | MySQL-generated invisible primary key behavior, metadata, and dump/replication interactions. |  |
| AUTO_INCREMENT columns | ❌ | Allocation, persistence, lock modes, explicit values, and overflow behavior. |  |
| Primary keys | ❌ | Definition forms, implicit NOT NULL behavior, metadata, and errors. |  |
| Unique indexes | ❌ | NULL handling, prefix lengths, functional key parts, and conflict semantics. |  |
| Nonunique indexes | ❌ | BTREE/HASH clauses, visibility, comments, and parser options. |  |
| Descending indexes | ❌ | DESC key-part syntax and ordering semantics. |  |
| Prefix indexes | ❌ | Prefix length parsing, byte/character semantics, and limits. |  |
| Functional key parts | ❌ | Expression key parts and metadata. |  |
| Multi-valued indexes | ❌ | JSON array multi-valued index syntax, casts, optimizer behavior, and metadata. |  |
| FULLTEXT indexes | ❌ | Index DDL, parser options, MATCH metadata, and embedded search behavior. |  |
| SPATIAL indexes | ❌ | Geometry column requirements and spatial metadata. |  |
| Foreign keys | ❌ | Definition, matching, cascades, restrict/no action, set null/default, checks, and metadata. |  |
| CHECK constraints | ❌ | Expression validation, enforcement, names, and metadata. |  |
| Constraint naming | ❌ | Generated names, duplicate handling, schema scope, and SHOW CREATE output. |  |
| Table options: engine | ❌ | ENGINE, SECONDARY_ENGINE, engine attributes, and unsupported-engine diagnostics. |  |
| Table options: charset/collation | ❌ | DEFAULT CHARACTER SET, COLLATE, and conversion-sensitive metadata. |  |
| Table options: storage | ❌ | ROW_FORMAT, COMPRESSION, ENCRYPTION, TABLESPACE, KEY_BLOCK_SIZE, and directories. |  |
| Table options: statistics | ❌ | STATS_AUTO_RECALC, STATS_PERSISTENT, STATS_SAMPLE_PAGES, AVG_ROW_LENGTH, MAX_ROWS, MIN_ROWS. |  |
| Table options: misc | ❌ | AUTO_INCREMENT, COMMENT, CHECKSUM, DELAY_KEY_WRITE, INSERT_METHOD, PACK_KEYS, PASSWORD, UNION. |  |
| NDB comment options | ❌ | NDB-specific table and column comment options recognized by MySQL grammar. |  |
| Temporary table metadata | ❌ | Session isolation, name shadowing, and cleanup. |  |
| CREATE INDEX options | ❌ | ALGORITHM, LOCK, visibility, comments, and index type clauses. |  |
| CREATE VIEW options | ❌ | ALGORITHM, DEFINER, SQL SECURITY, column list, and WITH CHECK OPTION. |  |

### 3.2 ALTER TABLE Actions

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `ADD COLUMN` | ❌ | Positioning, FIRST/AFTER, multiple columns, generated/invisible/default handling. |  |
| `DROP COLUMN` | ❌ | Dependency checks, generated columns, indexes, constraints, and errors. |  |
| `RENAME COLUMN` | ❌ | Metadata rewrite and dependency updates. |  |
| `CHANGE COLUMN` | ❌ | Rename plus type/attribute change semantics. |  |
| `MODIFY COLUMN` | ❌ | Type/attribute change without rename. |  |
| `ALTER COLUMN SET DEFAULT` | ❌ | Default mutation semantics. |  |
| `ALTER COLUMN DROP DEFAULT` | ❌ | Default removal semantics. |  |
| `ALTER COLUMN SET VISIBLE` / `SET INVISIBLE` | ❌ | Column visibility changes and restrictions. |  |
| `ADD PRIMARY KEY` | ❌ | Primary key addition and data validation. |  |
| `DROP PRIMARY KEY` | ❌ | Primary key removal and auto-increment restrictions. |  |
| `ADD UNIQUE` | ❌ | Unique index addition and duplicate validation. |  |
| `ADD INDEX` / `ADD KEY` | ❌ | Secondary index addition and metadata. |  |
| `ADD FULLTEXT` | ❌ | Full-text index addition. |  |
| `ADD SPATIAL` | ❌ | Spatial index addition. |  |
| `DROP INDEX` / `DROP KEY` | ❌ | Index removal and constraint dependencies. |  |
| `RENAME INDEX` / `RENAME KEY` | ❌ | Index rename semantics. |  |
| `ALTER INDEX VISIBLE` / `INVISIBLE` | ❌ | Index visibility metadata and optimizer behavior. |  |
| `ADD CONSTRAINT CHECK` | ❌ | Check constraint addition and validation. |  |
| `DROP CHECK` | ❌ | Check constraint removal. |  |
| `ALTER CHECK ENFORCED` / `NOT ENFORCED` | ❌ | Check enforcement toggling. |  |
| `ADD CONSTRAINT FOREIGN KEY` | ❌ | Foreign key addition, validation, indexes, and actions. |  |
| `DROP FOREIGN KEY` | ❌ | Foreign key removal and metadata cleanup. |  |
| `DISABLE KEYS` / `ENABLE KEYS` | ❌ | MyISAM-style key maintenance syntax and diagnostics. |  |
| `RENAME TO` | ❌ | Table rename via ALTER TABLE. |  |
| `ORDER BY` | ❌ | Physical row ordering syntax and embedded-compatible behavior. |  |
| `CONVERT TO CHARACTER SET` | ❌ | Column/table charset and collation conversion semantics. |  |
| `DEFAULT CHARACTER SET` / `COLLATE` | ❌ | Table default charset/collation changes. |  |
| `FORCE` | ❌ | Forced table rebuild semantics. |  |
| `DISCARD TABLESPACE` | ❌ | Tablespace discard syntax. |  |
| `IMPORT TABLESPACE` | ❌ | Tablespace import syntax. |  |
| `ALGORITHM` | ❌ | DEFAULT, INSTANT, INPLACE, COPY handling and diagnostics. |  |
| `LOCK` | ❌ | DEFAULT, NONE, SHARED, EXCLUSIVE handling and diagnostics. |  |
| Partition maintenance | ❌ | ADD, DROP, DISCARD, IMPORT, TRUNCATE, COALESCE, REORGANIZE, EXCHANGE, ANALYZE, CHECK, OPTIMIZE, REBUILD, REPAIR, REMOVE PARTITIONING. |  |

### 3.3 Partitioning Surface

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `PARTITION BY RANGE` | ❌ | Range partition syntax, VALUES LESS THAN, MAXVALUE, and pruning-visible metadata. |  |
| `PARTITION BY RANGE COLUMNS` | ❌ | Range columns syntax and comparison semantics. |  |
| `PARTITION BY LIST` | ❌ | List partition syntax and value matching. |  |
| `PARTITION BY LIST COLUMNS` | ❌ | List columns syntax and tuple matching. |  |
| `PARTITION BY HASH` | ❌ | Hash partition syntax and function semantics. |  |
| `PARTITION BY LINEAR HASH` | ❌ | Linear hash partition syntax. |  |
| `PARTITION BY KEY` | ❌ | Key partition syntax and default key selection. |  |
| `PARTITION BY LINEAR KEY` | ❌ | Linear key partition syntax. |  |
| Subpartitioning | ❌ | HASH/KEY subpartition syntax and metadata. |  |
| Partition options | ❌ | ENGINE, COMMENT, DATA/INDEX DIRECTORY, MAX_ROWS, MIN_ROWS, TABLESPACE, and nodegroup syntax. |  |

## 4. Type System, Literals, and Conversion

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `TINYINT` | ❌ | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. |  |
| `SMALLINT` | ❌ | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. |  |
| `MEDIUMINT` | ❌ | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. |  |
| `INT` / `INTEGER` | ❌ | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. |  |
| `BIGINT` | ❌ | Signed/unsigned integer range, overflow, unsigned arithmetic, and metadata. |  |
| Integer type aliases | ❌ | MySQL creation-time rewrites for `INT1`, `INT2`, `INT3`, `INT4`, `INT8`, and `MIDDLEINT`, including DESCRIBE and SHOW CREATE metadata. | `INT3` and `MIDDLEINT` map to `MEDIUMINT`. |
| `DECIMAL` / `NUMERIC` | ❌ | Exact precision math, storage limits, rounding, overflow, and metadata. |  |
| `FIXED` | ❌ | Alias behavior for `DECIMAL`, including creation-time metadata rewrites. | Other-vendor compatibility type. |
| `FLOAT` | ❌ | Approximate numeric parsing, rounding, special cases, and metadata. |  |
| `DOUBLE` / `REAL` | ❌ | Approximate numeric parsing, rounding, REAL_AS_FLOAT mode, and metadata. |  |
| `FLOAT4` / `FLOAT8` | ❌ | Alias behavior for `FLOAT` and `DOUBLE`, including creation-time metadata rewrites. | Other-vendor compatibility types. |
| `BIT` | ❌ | Bit-value storage, literals, display, and numeric/string conversion. |  |
| `BOOL` / `BOOLEAN` | ❌ | Alias behavior for TINYINT(1) and expression truth rules. |  |
| `SERIAL` | ❌ | Alias expansion to BIGINT UNSIGNED NOT NULL AUTO_INCREMENT UNIQUE. |  |
| `DATE` | ❌ | Date range, zero dates, invalid dates, literals, casts, and formatting. |  |
| `TIME` | ❌ | Time range, fractional seconds, negative values, casts, and formatting. |  |
| `DATETIME` | ❌ | Datetime range, fractional seconds, defaults, casts, and formatting. |  |
| `TIMESTAMP` | ❌ | UTC conversion, range, fractional seconds, defaults, and ON UPDATE. |  |
| `YEAR` | ❌ | Year storage, two-digit handling, casts, and display. |  |
| `CHAR` | ❌ | Fixed-length string semantics, padding, charsets, collations, and metadata. |  |
| `VARCHAR` | ❌ | Variable string semantics, length limits, charsets, collations, and metadata. |  |
| `CHARACTER` / `CHARACTER VARYING` | ❌ | Alias behavior for `CHAR` and `VARCHAR`, including creation-time metadata rewrites. |  |
| `NCHAR` / `NATIONAL CHAR` | ❌ | Standard national-character aliases using MySQL's predefined `utf8mb3` character set. |  |
| `NVARCHAR` / `NATIONAL VARCHAR` | ❌ | Standard national-character aliases using MySQL's predefined `utf8mb3` character set. |  |
| `BINARY` | ❌ | Fixed-length binary semantics and padding. |  |
| `VARBINARY` | ❌ | Variable binary semantics and length limits. |  |
| `CHAR BYTE` | ❌ | Alias behavior for `BINARY`, including metadata rewrites. |  |
| `TINYBLOB` | ❌ | Binary large object length, metadata, and comparison semantics. |  |
| `BLOB` | ❌ | Binary large object length, metadata, and comparison semantics. |  |
| `MEDIUMBLOB` | ❌ | Binary large object length, metadata, and comparison semantics. |  |
| `LONGBLOB` | ❌ | Binary large object length, metadata, and comparison semantics. |  |
| `TINYTEXT` | ❌ | Text length, charset/collation, metadata, and index-prefix semantics. |  |
| `TEXT` | ❌ | Text length, charset/collation, metadata, and index-prefix semantics. |  |
| `MEDIUMTEXT` | ❌ | Text length, charset/collation, metadata, and index-prefix semantics. |  |
| `LONGTEXT` | ❌ | Text length, charset/collation, metadata, and index-prefix semantics. |  |
| `LONG` / `LONG VARCHAR` | ❌ | Alias behavior for `MEDIUMTEXT`, including creation-time metadata rewrites. | Other-vendor compatibility types. |
| `LONG VARBINARY` | ❌ | Alias behavior for `MEDIUMBLOB`, including creation-time metadata rewrites. | Other-vendor compatibility type. |
| `ENUM` | ❌ | Element indexing, sorting, invalid values, empty string, metadata, and DDL changes. |  |
| `SET` | ❌ | Bitmap membership, ordering, invalid values, metadata, and DDL changes. |  |
| `JSON` | ❌ | Binary JSON semantics, validation, comparison, partial update metadata, and generated-column interactions. |  |
| `GEOMETRY` | ❌ | Base spatial type storage, SRID, validity, and metadata. |  |
| `POINT` | ❌ | Point storage, SRID, coordinate access, and metadata. |  |
| `LINESTRING` | ❌ | LineString storage, SRID, validity, and metadata. |  |
| `POLYGON` | ❌ | Polygon storage, SRID, validity, and metadata. |  |
| `MULTIPOINT` | ❌ | MultiPoint storage, SRID, validity, and metadata. |  |
| `MULTILINESTRING` | ❌ | MultiLineString storage, SRID, validity, and metadata. |  |
| `MULTIPOLYGON` | ❌ | MultiPolygon storage, SRID, validity, and metadata. |  |
| `GEOMETRYCOLLECTION` | ❌ | Geometry collection storage, SRID, validity, and metadata. |  |
| Numeric literals | ❌ | Decimal, hex, bit, scientific notation, signedness, and overflow conversion. |  |
| String literals | ❌ | Escapes, introducers, national character set, binary literals, sql_mode effects, and concatenation. |  |
| Temporal literals | ❌ | DATE/TIME/TIMESTAMP literal syntax and coercion. |  |
| JSON path literals | ❌ | Path grammar, quoting, wildcards, ranges, and errors. |  |
| User variables | ❌ | Type retention, coercion, assignment, charset/collation, and result metadata. |  |
| Local variables | ❌ | Stored-program variable typing, scope, and diagnostics. |  |
| Type conversion | ❌ | Expression, comparison, assignment, insert/update, aggregate, and function argument conversion rules. |  |
| Collation coercibility | ❌ | Coercibility ranks, illegal mix diagnostics, and result collation derivation. |  |

### 4.1 Character Sets

| Character set | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `armscii8` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `ascii` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `big5` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `binary` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp1250` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp1251` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp1256` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp1257` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp850` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp852` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp866` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp932` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `dec8` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `eucjpms` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `euckr` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `gb18030` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `gb2312` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `gbk` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `geostd8` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `greek` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `hebrew` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `hp8` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `keybcs2` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `koi8r` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `koi8u` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `latin1` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `latin2` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `latin5` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `latin7` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `macce` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `macroman` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `sjis` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `swe7` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `tis620` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `ucs2` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `ujis` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf8mb3` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf8mb4` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf16` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf16le` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf32` | ❌ | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |


### 4.2 Collations and Comparison Behavior

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| Collation catalog entries | ❌ | Expose every MySQL 8.4 collation through metadata, SHOW COLLATION, and charset/collation validation. |  |
| Default collation selection | ❌ | Match server, database, table, column, literal, and expression default collation resolution. |  |
| Unicode Collation Algorithm families | ❌ | Match utf8mb4_0900, unicode, language-specific, accent-sensitive, case-sensitive, and kana-sensitive behavior where MySQL exposes it. |  |
| Binary collations | ❌ | Match binary string ordering, equality, case sensitivity, and metadata flags. |  |
| PAD SPACE and NO PAD collations | ❌ | Match trailing-space comparison, uniqueness, ordering, and index behavior. |  |
| Collation coercibility rules | ❌ | Match coercibility ranks, illegal mix diagnostics, and result collation derivation. |  |
| WEIGHT_STRING behavior | ❌ | Match sort weight generation for supported collations and strings. |  |

## 5. Built-In Functions and Operators

This table is generated from the MySQL 8.4 built-in function and operator reference. Internal-only entries are listed because applications and metadata queries can still observe their names or diagnostics.

| Function or operator | Status | MySQL behavior to match | Implementation notes |
| --- | --- | --- | --- |
| `&` | ❌ | Bitwise AND |  |
| `>` | ❌ | Greater than operator |  |
| `>>` | ❌ | Right shift |  |
| `>=` | ❌ | Greater than or equal operator |  |
| `<` | ❌ | Less than operator |  |
| `<>, !=` | ❌ | Not equal operator |  |
| `<<` | ❌ | Left shift |  |
| `<=` | ❌ | Less than or equal operator |  |
| `<=>` | ❌ | NULL-safe equal to operator |  |
| `%, MOD` | ❌ | Modulo operator |  |
| `*` | ❌ | Multiplication operator |  |
| `+` | ❌ | Addition operator |  |
| `-` (binary) | ❌ | Minus operator |  |
| `-` (unary) | ❌ | Change the sign of the argument |  |
| `->` | ❌ | Return value from JSON column after evaluating path; equivalent to JSON_EXTRACT(). |  |
| `->>` | ❌ | Return value from JSON column after evaluating path and unquoting the result; equivalent to JSON_UNQUOTE(JSON_EXTRACT()). |  |
| `/` | ❌ | Division operator |  |
| `:=` | ❌ | Assign a value |  |
| `=` (assignment) | ❌ | Assign a value as part of `SET` or an `UPDATE` assignment. |  |
| `=` (comparison) | ❌ | Equal operator |  |
| `^` | ❌ | Bitwise XOR |  |
| `ABS()` | ❌ | Return the absolute value |  |
| `ACOS()` | ❌ | Return the arc cosine |  |
| `ADDDATE()` | ❌ | Add time values (intervals) to a date value |  |
| `ADDTIME()` | ❌ | Add time |  |
| `AES_DECRYPT()` | ❌ | Decrypt using AES |  |
| `AES_ENCRYPT()` | ❌ | Encrypt using AES |  |
| `AND, &&` | ❌ | Logical AND |  |
| `ANY_VALUE()` | ❌ | Suppress ONLY_FULL_GROUP_BY value rejection |  |
| `ASCII()` | ❌ | Return numeric value of left-most character |  |
| `ASIN()` | ❌ | Return the arc sine |  |
| `asynchronous_connection_failover_add_managed()` | ❌ | Add group member source server configuration information to a replication channel source list |  |
| `asynchronous_connection_failover_add_source()` | ❌ | Add source server configuration information server to a replication channel source list |  |
| `asynchronous_connection_failover_delete_managed()` | ❌ | Remove a managed group from a replication channel source list |  |
| `asynchronous_connection_failover_delete_source()` | ❌ | Remove a source server from a replication channel source list |  |
| `asynchronous_connection_failover_reset()` | ❌ | Remove all settings relating to group replication asynchronous failover |  |
| `ATAN()` | ❌ | Return the arc tangent |  |
| `ATAN2(), ATAN()` | ❌ | Return the arc tangent of the two arguments |  |
| `AVG()` | ❌ | Return the average value of the argument |  |
| `BENCHMARK()` | ❌ | Repeatedly execute an expression |  |
| `BETWEEN ... AND ...` | ❌ | Whether a value is within a range of values |  |
| `BIN()` | ❌ | Return a string containing binary representation of a number |  |
| `BIN_TO_UUID()` | ❌ | Convert binary UUID to string |  |
| `BINARY` | ❌ | Cast a string to a binary string |  |
| `BIT_AND()` | ❌ | Return bitwise AND |  |
| `BIT_COUNT()` | ❌ | Return the number of bits that are set |  |
| `BIT_LENGTH()` | ❌ | Return length of argument in bits |  |
| `BIT_OR()` | ❌ | Return bitwise OR |  |
| `BIT_XOR()` | ❌ | Return bitwise XOR |  |
| `CAN_ACCESS_COLUMN()` | ❌ | Internal use only |  |
| `CAN_ACCESS_DATABASE()` | ❌ | Internal use only |  |
| `CAN_ACCESS_TABLE()` | ❌ | Internal use only |  |
| `CAN_ACCESS_USER()` | ❌ | Internal use only |  |
| `CAN_ACCESS_VIEW()` | ❌ | Internal use only |  |
| `CASE` | ❌ | Case operator |  |
| `CAST()` | ❌ | Cast a value as a certain type |  |
| `CEIL()` | ❌ | Return the smallest integer value not less than the argument |  |
| `CEILING()` | ❌ | Return the smallest integer value not less than the argument |  |
| `CHAR()` | ❌ | Return the character for each integer passed |  |
| `CHAR_LENGTH()` | ❌ | Return number of characters in argument |  |
| `CHARACTER_LENGTH()` | ❌ | Synonym for CHAR_LENGTH() |  |
| `CHARSET()` | ❌ | Return the character set of the argument |  |
| `COALESCE()` | ❌ | Return the first non-NULL argument |  |
| `COERCIBILITY()` | ❌ | Return the collation coercibility value of the string argument |  |
| `COLLATION()` | ❌ | Return the collation of the string argument |  |
| `COMPRESS()` | ❌ | Return result as a binary string |  |
| `CONCAT()` | ❌ | Return concatenated string |  |
| `CONCAT_WS()` | ❌ | Return concatenate with separator |  |
| `CONNECTION_ID()` | ❌ | Return the connection ID (thread ID) for the connection |  |
| `CONV()` | ❌ | Convert numbers between different number bases |  |
| `CONVERT()` | ❌ | Cast a value as a certain type |  |
| `CONVERT_TZ()` | ❌ | Convert from one time zone to another |  |
| `COS()` | ❌ | Return the cosine |  |
| `COT()` | ❌ | Return the cotangent |  |
| `COUNT()` | ❌ | Return a count of the number of rows returned |  |
| `COUNT(DISTINCT)` | ❌ | Return the count of a number of different values |  |
| `CRC32()` | ❌ | Compute a cyclic redundancy check value |  |
| `CUME_DIST()` | ❌ | Cumulative distribution value |  |
| `CURDATE()` | ❌ | Return the current date |  |
| `CURRENT_DATE(), CURRENT_DATE` | ❌ | Synonyms for CURDATE() |  |
| `CURRENT_ROLE()` | ❌ | Return the current active roles |  |
| `CURRENT_TIME(), CURRENT_TIME` | ❌ | Synonyms for CURTIME() |  |
| `CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP` | ❌ | Synonyms for NOW() |  |
| `CURRENT_USER(), CURRENT_USER` | ❌ | The authenticated user name and host name |  |
| `CURTIME()` | ❌ | Return the current time |  |
| `DATABASE()` | ❌ | Return the default (current) database name |  |
| `DATE()` | ❌ | Extract the date part of a date or datetime expression |  |
| `DATE_ADD()` | ❌ | Add time values (intervals) to a date value |  |
| `DATE_FORMAT()` | ❌ | Format date as specified |  |
| `DATE_SUB()` | ❌ | Subtract a time value (interval) from a date |  |
| `DATEDIFF()` | ❌ | Subtract two dates |  |
| `DAY()` | ❌ | Synonym for DAYOFMONTH() |  |
| `DAYNAME()` | ❌ | Return the name of the weekday |  |
| `DAYOFMONTH()` | ❌ | Return the day of the month (0-31) |  |
| `DAYOFWEEK()` | ❌ | Return the weekday index of the argument |  |
| `DAYOFYEAR()` | ❌ | Return the day of the year (1-366) |  |
| `DEFAULT()` | ❌ | Return the default value for a table column |  |
| `DEGREES()` | ❌ | Convert radians to degrees |  |
| `DENSE_RANK()` | ❌ | Rank of current row within its partition, without gaps |  |
| `DIV` | ❌ | Integer division |  |
| `ELT()` | ❌ | Return string at index number |  |
| `EXISTS()` | ❌ | Whether the result of a query contains any rows |  |
| `EXP()` | ❌ | Raise to the power of |  |
| `EXPORT_SET()` | ❌ | Return a string such that for every bit set in the value bits, you get an on string and for every unset bit, you get an off string |  |
| `EXTRACT()` | ❌ | Extract part of a date |  |
| `ExtractValue()` | ❌ | Extract a value from an XML string using XPath notation |  |
| `FIELD()` | ❌ | Index (position) of first argument in subsequent arguments |  |
| `FIND_IN_SET()` | ❌ | Index (position) of first argument within second argument |  |
| `FIRST_VALUE()` | ❌ | Value of argument from first row of window frame |  |
| `FLOOR()` | ❌ | Return the largest integer value not greater than the argument |  |
| `FORMAT()` | ❌ | Return a number formatted to specified number of decimal places |  |
| `FORMAT_BYTES()` | ❌ | Convert byte count to value with units |  |
| `FORMAT_PICO_TIME()` | ❌ | Convert time in picoseconds to value with units |  |
| `FOUND_ROWS()` | ❌ | For a SELECT with a LIMIT clause, the number of rows that would be returned were there no LIMIT clause |  |
| `FROM_BASE64()` | ❌ | Decode base64 encoded string and return result |  |
| `FROM_DAYS()` | ❌ | Convert a day number to a date |  |
| `FROM_UNIXTIME()` | ❌ | Format Unix timestamp as a date |  |
| `GeomCollection()` | ❌ | Construct geometry collection from geometries |  |
| `GeometryCollection()` | ❌ | Construct geometry collection from geometries |  |
| `GET_DD_COLUMN_PRIVILEGES()` | ❌ | Internal use only |  |
| `GET_DD_CREATE_OPTIONS()` | ❌ | Internal use only |  |
| `GET_DD_INDEX_SUB_PART_LENGTH()` | ❌ | Internal use only |  |
| `GET_FORMAT()` | ❌ | Return a date format string |  |
| `GET_LOCK()` | ❌ | Get a named lock |  |
| `GREATEST()` | ❌ | Return the largest argument |  |
| `GROUP_CONCAT()` | ❌ | Return a concatenated string |  |
| `group_replication_disable_member_action()` | ❌ | Disable member action for event specified |  |
| `group_replication_enable_member_action()` | ❌ | Enable member action for event specified |  |
| `group_replication_get_communication_protocol()` | ❌ | Get version of group replication communication protocol currently in use |  |
| `group_replication_get_write_concurrency()` | ❌ | Get maximum number of consensus instances currently set for group |  |
| `group_replication_reset_member_actions()` | ❌ | Reset all member actions to defaults and configuration version number to 1 |  |
| `group_replication_set_as_primary()` | ❌ | Make a specific group member the primary |  |
| `group_replication_set_communication_protocol()` | ❌ | Set version for group replication communication protocol to use |  |
| `group_replication_set_write_concurrency()` | ❌ | Set maximum number of consensus instances that can be executed in parallel |  |
| `group_replication_switch_to_multi_primary_mode()` | ❌ | Changes the mode of a group running in single-primary mode to multi-primary mode |  |
| `group_replication_switch_to_single_primary_mode()` | ❌ | Changes the mode of a group running in multi-primary mode to single-primary mode |  |
| `GROUPING()` | ❌ | Distinguish super-aggregate ROLLUP rows from regular rows |  |
| `HEX()` | ❌ | Hexadecimal representation of decimal or string value |  |
| `HOUR()` | ❌ | Extract the hour |  |
| `ICU_VERSION()` | ❌ | ICU library version |  |
| `IF()` | ❌ | If/else construct |  |
| `IFNULL()` | ❌ | Null if/else construct |  |
| `IN()` | ❌ | Whether a value is within a set of values |  |
| `INET_ATON()` | ❌ | Return the numeric value of an IP address |  |
| `INET_NTOA()` | ❌ | Return the IP address from a numeric value |  |
| `INSERT()` | ❌ | Insert substring at specified position up to specified number of characters |  |
| `INSTR()` | ❌ | Return the index of the first occurrence of substring |  |
| `INTERNAL_AUTO_INCREMENT()` | ❌ | Internal use only |  |
| `INTERNAL_AVG_ROW_LENGTH()` | ❌ | Internal use only |  |
| `INTERNAL_CHECK_TIME()` | ❌ | Internal use only |  |
| `INTERNAL_CHECKSUM()` | ❌ | Internal use only |  |
| `INTERNAL_DATA_FREE()` | ❌ | Internal use only |  |
| `INTERNAL_DATA_LENGTH()` | ❌ | Internal use only |  |
| `INTERNAL_DD_CHAR_LENGTH()` | ❌ | Internal use only |  |
| `INTERNAL_GET_COMMENT_OR_ERROR()` | ❌ | Internal use only |  |
| `INTERNAL_GET_ENABLED_ROLE_JSON()` | ❌ | Internal use only |  |
| `INTERNAL_GET_HOSTNAME()` | ❌ | Internal use only |  |
| `INTERNAL_GET_USERNAME()` | ❌ | Internal use only |  |
| `INTERNAL_GET_VIEW_WARNING_OR_ERROR()` | ❌ | Internal use only |  |
| `INTERNAL_INDEX_COLUMN_CARDINALITY()` | ❌ | Internal use only |  |
| `INTERNAL_INDEX_LENGTH()` | ❌ | Internal use only |  |
| `INTERNAL_IS_ENABLED_ROLE()` | ❌ | Internal use only |  |
| `INTERNAL_IS_MANDATORY_ROLE()` | ❌ | Internal use only |  |
| `INTERNAL_KEYS_DISABLED()` | ❌ | Internal use only |  |
| `INTERNAL_MAX_DATA_LENGTH()` | ❌ | Internal use only |  |
| `INTERNAL_TABLE_ROWS()` | ❌ | Internal use only |  |
| `INTERNAL_UPDATE_TIME()` | ❌ | Internal use only |  |
| `INTERVAL()` | ❌ | Return the index of the argument that is less than the first argument |  |
| `IS` | ❌ | Test a value against a boolean |  |
| `IS_FREE_LOCK()` | ❌ | Whether the named lock is free |  |
| `IS NOT` | ❌ | Test a value against a boolean |  |
| `IS NOT NULL` | ❌ | NOT NULL value test |  |
| `IS NULL` | ❌ | NULL value test |  |
| `IS_USED_LOCK()` | ❌ | Whether the named lock is in use; return connection identifier if true |  |
| `IS_UUID()` | ❌ | Whether argument is a valid UUID |  |
| `ISNULL()` | ❌ | Test whether the argument is NULL |  |
| `JSON_ARRAY()` | ❌ | Create JSON array |  |
| `JSON_ARRAY_APPEND()` | ❌ | Append data to JSON document |  |
| `JSON_ARRAY_INSERT()` | ❌ | Insert into JSON array |  |
| `JSON_ARRAYAGG()` | ❌ | Return result set as a single JSON array |  |
| `JSON_CONTAINS()` | ❌ | Whether JSON document contains specific object at path |  |
| `JSON_CONTAINS_PATH()` | ❌ | Whether JSON document contains any data at path |  |
| `JSON_DEPTH()` | ❌ | Maximum depth of JSON document |  |
| `JSON_EXTRACT()` | ❌ | Return data from JSON document |  |
| `JSON_INSERT()` | ❌ | Insert data into JSON document |  |
| `JSON_KEYS()` | ❌ | Array of keys from JSON document |  |
| `JSON_LENGTH()` | ❌ | Number of elements in JSON document |  |
| `JSON_MERGE()` | ❌ | Merge JSON documents, preserving duplicate keys. Deprecated synonym for JSON_MERGE_PRESERVE() |  |
| `JSON_MERGE_PATCH()` | ❌ | Merge JSON documents, replacing values of duplicate keys |  |
| `JSON_MERGE_PRESERVE()` | ❌ | Merge JSON documents, preserving duplicate keys |  |
| `JSON_OBJECT()` | ❌ | Create JSON object |  |
| `JSON_OBJECTAGG()` | ❌ | Return result set as a single JSON object |  |
| `JSON_OVERLAPS()` | ❌ | Compares two JSON documents, returns TRUE (1) if these have any key-value pairs or array elements in common, otherwise FALSE (0) |  |
| `JSON_PRETTY()` | ❌ | Print a JSON document in human-readable format |  |
| `JSON_QUOTE()` | ❌ | Quote JSON document |  |
| `JSON_REMOVE()` | ❌ | Remove data from JSON document |  |
| `JSON_REPLACE()` | ❌ | Replace values in JSON document |  |
| `JSON_SCHEMA_VALID()` | ❌ | Validate JSON document against JSON schema; returns TRUE/1 if document validates against schema, or FALSE/0 if it does not |  |
| `JSON_SCHEMA_VALIDATION_REPORT()` | ❌ | Validate JSON document against JSON schema; returns report in JSON format on outcome on validation including success or failure and reasons for failure |  |
| `JSON_SEARCH()` | ❌ | Path to value within JSON document |  |
| `JSON_SET()` | ❌ | Insert data into JSON document |  |
| `JSON_STORAGE_FREE()` | ❌ | Freed space within binary representation of JSON column value following partial update |  |
| `JSON_STORAGE_SIZE()` | ❌ | Space used for storage of binary representation of a JSON document |  |
| `JSON_TABLE()` | ❌ | Return data from a JSON expression as a relational table |  |
| `JSON_TYPE()` | ❌ | Type of JSON value |  |
| `JSON_UNQUOTE()` | ❌ | Unquote JSON value |  |
| `JSON_VALID()` | ❌ | Whether JSON value is valid |  |
| `JSON_VALUE()` | ❌ | Extract value from JSON document at location pointed to by path provided; return this value as VARCHAR(512) or specified type |  |
| `LAG()` | ❌ | Value of argument from row lagging current row within partition |  |
| `LAST_DAY` | ❌ | Return the last day of the month for the argument |  |
| `LAST_INSERT_ID()` | ❌ | Value of the AUTOINCREMENT column for the last INSERT |  |
| `LAST_VALUE()` | ❌ | Value of argument from last row of window frame |  |
| `LCASE()` | ❌ | Synonym for LOWER() |  |
| `LEAD()` | ❌ | Value of argument from row leading current row within partition |  |
| `LEAST()` | ❌ | Return the smallest argument |  |
| `LEFT()` | ❌ | Return the leftmost number of characters as specified |  |
| `LENGTH()` | ❌ | Return the length of a string in bytes |  |
| `LIKE` | ❌ | Simple pattern matching |  |
| `LineString()` | ❌ | Construct LineString from Point values |  |
| `LN()` | ❌ | Return the natural logarithm of the argument |  |
| `LOAD_FILE()` | ❌ | Load the named file |  |
| `LOCALTIME(), LOCALTIME` | ❌ | Synonym for NOW() |  |
| `LOCALTIMESTAMP, LOCALTIMESTAMP()` | ❌ | Synonym for NOW() |  |
| `LOCATE()` | ❌ | Return the position of the first occurrence of substring |  |
| `LOG()` | ❌ | Return the natural logarithm of the first argument |  |
| `LOG10()` | ❌ | Return the base-10 logarithm of the argument |  |
| `LOG2()` | ❌ | Return the base-2 logarithm of the argument |  |
| `LOWER()` | ❌ | Return the argument in lowercase |  |
| `LPAD()` | ❌ | Return the string argument, left-padded with the specified string |  |
| `LTRIM()` | ❌ | Remove leading spaces |  |
| `MAKE_SET()` | ❌ | Return a set of comma-separated strings that have the corresponding bit in bits set |  |
| `MAKEDATE()` | ❌ | Create a date from the year and day of year |  |
| `MAKETIME()` | ❌ | Create time from hour, minute, second |  |
| `MASTER_POS_WAIT()` | ❌ | Block until the replica has read and applied all updates up to the specified position |  |
| `MATCH()` | ❌ | Perform full-text search |  |
| `MAX()` | ❌ | Return the maximum value |  |
| `MBRContains()` | ❌ | Whether MBR of one geometry contains MBR of another |  |
| `MBRCoveredBy()` | ❌ | Whether one MBR is covered by another |  |
| `MBRCovers()` | ❌ | Whether one MBR covers another |  |
| `MBRDisjoint()` | ❌ | Whether MBRs of two geometries are disjoint |  |
| `MBREquals()` | ❌ | Whether MBRs of two geometries are equal |  |
| `MBRIntersects()` | ❌ | Whether MBRs of two geometries intersect |  |
| `MBROverlaps()` | ❌ | Whether MBRs of two geometries overlap |  |
| `MBRTouches()` | ❌ | Whether MBRs of two geometries touch |  |
| `MBRWithin()` | ❌ | Whether MBR of one geometry is within MBR of another |  |
| `MD5()` | ❌ | Calculate MD5 checksum |  |
| `MEMBER OF()` | ❌ | Returns true (1) if first operand matches any element of JSON array passed as second operand, otherwise returns false (0) |  |
| `MICROSECOND()` | ❌ | Return the microseconds from argument |  |
| `MID()` | ❌ | Return a substring starting from the specified position |  |
| `MIN()` | ❌ | Return the minimum value |  |
| `MINUTE()` | ❌ | Return the minute from the argument |  |
| `MOD()` | ❌ | Return the remainder |  |
| `MONTH()` | ❌ | Return the month from the date passed |  |
| `MONTHNAME()` | ❌ | Return the name of the month |  |
| `MultiLineString()` | ❌ | Contruct MultiLineString from LineString values |  |
| `MultiPoint()` | ❌ | Construct MultiPoint from Point values |  |
| `MultiPolygon()` | ❌ | Construct MultiPolygon from Polygon values |  |
| `NAME_CONST()` | ❌ | Cause the column to have the given name |  |
| `NOT, !` | ❌ | Negates value |  |
| `NOT BETWEEN ... AND ...` | ❌ | Whether a value is not within a range of values |  |
| `NOT EXISTS()` | ❌ | Whether the result of a query contains no rows |  |
| `NOT IN()` | ❌ | Whether a value is not within a set of values |  |
| `NOT LIKE` | ❌ | Negation of simple pattern matching |  |
| `NOT REGEXP` | ❌ | Negation of REGEXP |  |
| `NOW()` | ❌ | Return the current date and time |  |
| `NTH_VALUE()` | ❌ | Value of argument from N-th row of window frame |  |
| `NTILE()` | ❌ | Bucket number of current row within its partition. |  |
| `NULLIF()` | ❌ | Return NULL if expr1 = expr2 |  |
| `OCT()` | ❌ | Return a string containing octal representation of a number |  |
| `OCTET_LENGTH()` | ❌ | Synonym for LENGTH() |  |
| `OR, \|\|` | ❌ | Logical OR |  |
| `ORD()` | ❌ | Return character code for leftmost character of the argument |  |
| `PERCENT_RANK()` | ❌ | Percentage rank value |  |
| `PERIOD_ADD()` | ❌ | Add a period to a year-month |  |
| `PERIOD_DIFF()` | ❌ | Return the number of months between periods |  |
| `PI()` | ❌ | Return the value of pi |  |
| `Point()` | ❌ | Construct Point from coordinates |  |
| `Polygon()` | ❌ | Construct Polygon from LineString arguments |  |
| `POSITION()` | ❌ | Synonym for LOCATE() |  |
| `POW()` | ❌ | Return the argument raised to the specified power |  |
| `POWER()` | ❌ | Return the argument raised to the specified power |  |
| `PS_CURRENT_THREAD_ID()` | ❌ | Performance Schema thread ID for current thread |  |
| `PS_THREAD_ID()` | ❌ | Performance Schema thread ID for given thread |  |
| `QUARTER()` | ❌ | Return the quarter from a date argument |  |
| `QUOTE()` | ❌ | Escape the argument for use in an SQL statement |  |
| `RADIANS()` | ❌ | Return argument converted to radians |  |
| `RAND()` | ❌ | Return a random floating-point value |  |
| `RANDOM_BYTES()` | ❌ | Return a random byte vector |  |
| `RANK()` | ❌ | Rank of current row within its partition, with gaps |  |
| `REGEXP` | ❌ | Whether string matches regular expression |  |
| `REGEXP_INSTR()` | ❌ | Starting index of substring matching regular expression |  |
| `REGEXP_LIKE()` | ❌ | Whether string matches regular expression |  |
| `REGEXP_REPLACE()` | ❌ | Replace substrings matching regular expression |  |
| `REGEXP_SUBSTR()` | ❌ | Return substring matching regular expression |  |
| `RELEASE_ALL_LOCKS()` | ❌ | Release all current named locks |  |
| `RELEASE_LOCK()` | ❌ | Release the named lock |  |
| `REPEAT()` | ❌ | Repeat a string the specified number of times |  |
| `REPLACE()` | ❌ | Replace occurrences of a specified string |  |
| `REVERSE()` | ❌ | Reverse the characters in a string |  |
| `RIGHT()` | ❌ | Return the specified rightmost number of characters |  |
| `RLIKE` | ❌ | Whether string matches regular expression |  |
| `ROLES_GRAPHML()` | ❌ | Return a GraphML document representing memory role subgraphs |  |
| `ROUND()` | ❌ | Round the argument |  |
| `ROW_COUNT()` | ❌ | The number of rows updated |  |
| `ROW_NUMBER()` | ❌ | Number of current row within its partition |  |
| `RPAD()` | ❌ | Append string the specified number of times |  |
| `RTRIM()` | ❌ | Remove trailing spaces |  |
| `SCHEMA()` | ❌ | Synonym for DATABASE() |  |
| `SEC_TO_TIME()` | ❌ | Converts seconds to 'hh:mm:ss' format |  |
| `SECOND()` | ❌ | Return the second (0-59) |  |
| `SESSION_USER()` | ❌ | Synonym for USER() |  |
| `SHA1(), SHA()` | ❌ | Calculate an SHA-1 160-bit checksum |  |
| `SHA2()` | ❌ | Calculate an SHA-2 checksum |  |
| `SIGN()` | ❌ | Return the sign of the argument |  |
| `SIN()` | ❌ | Return the sine of the argument |  |
| `SLEEP()` | ❌ | Sleep for a number of seconds |  |
| `SOUNDEX()` | ❌ | Return a soundex string |  |
| `SOUNDS LIKE` | ❌ | Compare sounds |  |
| `SOURCE_POS_WAIT()` | ❌ | Block until the replica has read and applied all updates up to the specified position |  |
| `SPACE()` | ❌ | Return a string of the specified number of spaces |  |
| `SQRT()` | ❌ | Return the square root of the argument |  |
| `ST_Area()` | ❌ | Return Polygon or MultiPolygon area |  |
| `ST_AsBinary(), ST_AsWKB()` | ❌ | Convert from internal geometry format to WKB |  |
| `ST_AsGeoJSON()` | ❌ | Generate GeoJSON object from geometry |  |
| `ST_AsText(), ST_AsWKT()` | ❌ | Convert from internal geometry format to WKT |  |
| `ST_Buffer()` | ❌ | Return geometry of points within given distance from geometry |  |
| `ST_Buffer_Strategy()` | ❌ | Produce strategy option for ST_Buffer() |  |
| `ST_Centroid()` | ❌ | Return centroid as a point |  |
| `ST_Collect()` | ❌ | Aggregate spatial values into collection |  |
| `ST_Contains()` | ❌ | Whether one geometry contains another |  |
| `ST_ConvexHull()` | ❌ | Return convex hull of geometry |  |
| `ST_Crosses()` | ❌ | Whether one geometry crosses another |  |
| `ST_Difference()` | ❌ | Return point set difference of two geometries |  |
| `ST_Dimension()` | ❌ | Dimension of geometry |  |
| `ST_Disjoint()` | ❌ | Whether one geometry is disjoint from another |  |
| `ST_Distance()` | ❌ | The distance of one geometry from another |  |
| `ST_Distance_Sphere()` | ❌ | Minimum distance on earth between two geometries |  |
| `ST_EndPoint()` | ❌ | End Point of LineString |  |
| `ST_Envelope()` | ❌ | Return MBR of geometry |  |
| `ST_Equals()` | ❌ | Whether one geometry is equal to another |  |
| `ST_ExteriorRing()` | ❌ | Return exterior ring of Polygon |  |
| `ST_FrechetDistance()` | ❌ | The discrete Fréchet distance of one geometry from another |  |
| `ST_GeoHash()` | ❌ | Produce a geohash value |  |
| `ST_GeomCollFromText(), ST_GeometryCollectionFromText(), ST_GeomCollFromTxt()` | ❌ | Return geometry collection from WKT |  |
| `ST_GeomCollFromWKB(), ST_GeometryCollectionFromWKB()` | ❌ | Return geometry collection from WKB |  |
| `ST_GeometryN()` | ❌ | Return N-th geometry from geometry collection |  |
| `ST_GeometryType()` | ❌ | Return name of geometry type |  |
| `ST_GeomFromGeoJSON()` | ❌ | Generate geometry from GeoJSON object |  |
| `ST_GeomFromText(), ST_GeometryFromText()` | ❌ | Return geometry from WKT |  |
| `ST_GeomFromWKB(), ST_GeometryFromWKB()` | ❌ | Return geometry from WKB |  |
| `ST_HausdorffDistance()` | ❌ | The discrete Hausdorff distance of one geometry from another |  |
| `ST_InteriorRingN()` | ❌ | Return N-th interior ring of Polygon |  |
| `ST_Intersection()` | ❌ | Return point set intersection of two geometries |  |
| `ST_Intersects()` | ❌ | Whether one geometry intersects another |  |
| `ST_IsClosed()` | ❌ | Whether a geometry is closed and simple |  |
| `ST_IsEmpty()` | ❌ | Whether a geometry is empty |  |
| `ST_IsSimple()` | ❌ | Whether a geometry is simple |  |
| `ST_IsValid()` | ❌ | Whether a geometry is valid |  |
| `ST_LatFromGeoHash()` | ❌ | Return latitude from geohash value |  |
| `ST_Latitude()` | ❌ | Return latitude of Point |  |
| `ST_Length()` | ❌ | Return length of LineString |  |
| `ST_LineFromText(), ST_LineStringFromText()` | ❌ | Construct LineString from WKT |  |
| `ST_LineFromWKB(), ST_LineStringFromWKB()` | ❌ | Construct LineString from WKB |  |
| `ST_LineInterpolatePoint()` | ❌ | The point a given percentage along a LineString |  |
| `ST_LineInterpolatePoints()` | ❌ | The points a given percentage along a LineString |  |
| `ST_LongFromGeoHash()` | ❌ | Return longitude from geohash value |  |
| `ST_Longitude()` | ❌ | Return longitude of Point |  |
| `ST_MakeEnvelope()` | ❌ | Rectangle around two points |  |
| `ST_MLineFromText(), ST_MultiLineStringFromText()` | ❌ | Construct MultiLineString from WKT |  |
| `ST_MLineFromWKB(), ST_MultiLineStringFromWKB()` | ❌ | Construct MultiLineString from WKB |  |
| `ST_MPointFromText(), ST_MultiPointFromText()` | ❌ | Construct MultiPoint from WKT |  |
| `ST_MPointFromWKB(), ST_MultiPointFromWKB()` | ❌ | Construct MultiPoint from WKB |  |
| `ST_MPolyFromText(), ST_MultiPolygonFromText()` | ❌ | Construct MultiPolygon from WKT |  |
| `ST_MPolyFromWKB(), ST_MultiPolygonFromWKB()` | ❌ | Construct MultiPolygon from WKB |  |
| `ST_NumGeometries()` | ❌ | Return number of geometries in geometry collection |  |
| `ST_NumInteriorRing(), ST_NumInteriorRings()` | ❌ | Return number of interior rings in Polygon |  |
| `ST_NumPoints()` | ❌ | Return number of points in LineString |  |
| `ST_Overlaps()` | ❌ | Whether one geometry overlaps another |  |
| `ST_PointAtDistance()` | ❌ | The point a given distance along a LineString |  |
| `ST_PointFromGeoHash()` | ❌ | Convert geohash value to POINT value |  |
| `ST_PointFromText()` | ❌ | Construct Point from WKT |  |
| `ST_PointFromWKB()` | ❌ | Construct Point from WKB |  |
| `ST_PointN()` | ❌ | Return N-th point from LineString |  |
| `ST_PolyFromText(), ST_PolygonFromText()` | ❌ | Construct Polygon from WKT |  |
| `ST_PolyFromWKB(), ST_PolygonFromWKB()` | ❌ | Construct Polygon from WKB |  |
| `ST_Simplify()` | ❌ | Return simplified geometry |  |
| `ST_SRID()` | ❌ | Return spatial reference system ID for geometry |  |
| `ST_StartPoint()` | ❌ | Start Point of LineString |  |
| `ST_SwapXY()` | ❌ | Return argument with X/Y coordinates swapped |  |
| `ST_SymDifference()` | ❌ | Return point set symmetric difference of two geometries |  |
| `ST_Touches()` | ❌ | Whether one geometry touches another |  |
| `ST_Transform()` | ❌ | Transform coordinates of geometry |  |
| `ST_Union()` | ❌ | Return point set union of two geometries |  |
| `ST_Validate()` | ❌ | Return validated geometry |  |
| `ST_Within()` | ❌ | Whether one geometry is within another |  |
| `ST_X()` | ❌ | Return X coordinate of Point |  |
| `ST_Y()` | ❌ | Return Y coordinate of Point |  |
| `STATEMENT_DIGEST()` | ❌ | Compute statement digest hash value |  |
| `STATEMENT_DIGEST_TEXT()` | ❌ | Compute normalized statement digest |  |
| `STD()` | ❌ | Return the population standard deviation |  |
| `STDDEV()` | ❌ | Return the population standard deviation |  |
| `STDDEV_POP()` | ❌ | Return the population standard deviation |  |
| `STDDEV_SAMP()` | ❌ | Return the sample standard deviation |  |
| `STR_TO_DATE()` | ❌ | Convert a string to a date |  |
| `STRCMP()` | ❌ | Compare two strings |  |
| `SUBDATE()` | ❌ | Synonym for DATE_SUB() when invoked with three arguments |  |
| `SUBSTR()` | ❌ | Return the substring as specified |  |
| `SUBSTRING()` | ❌ | Return the substring as specified |  |
| `SUBSTRING_INDEX()` | ❌ | Return a substring from a string before the specified number of occurrences of the delimiter |  |
| `SUBTIME()` | ❌ | Subtract times |  |
| `SUM()` | ❌ | Return the sum |  |
| `SYSDATE()` | ❌ | Return the time at which the function executes |  |
| `SYSTEM_USER()` | ❌ | Synonym for USER() |  |
| `TAN()` | ❌ | Return the tangent of the argument |  |
| `TIME()` | ❌ | Extract the time portion of the expression passed |  |
| `TIME_FORMAT()` | ❌ | Format as time |  |
| `TIME_TO_SEC()` | ❌ | Return the argument converted to seconds |  |
| `TIMEDIFF()` | ❌ | Subtract time |  |
| `TIMESTAMP()` | ❌ | With a single argument, this function returns the date or datetime expression; with two arguments, the sum of the arguments |  |
| `TIMESTAMPADD()` | ❌ | Add an interval to a datetime expression |  |
| `TIMESTAMPDIFF()` | ❌ | Return the difference of two datetime expressions, using the units specified |  |
| `TO_BASE64()` | ❌ | Return the argument converted to a base-64 string |  |
| `TO_DAYS()` | ❌ | Return the date argument converted to days |  |
| `TO_SECONDS()` | ❌ | Return the date or datetime argument converted to seconds since Year 0 |  |
| `TRIM()` | ❌ | Remove leading and trailing spaces |  |
| `TRUNCATE()` | ❌ | Truncate to specified number of decimal places |  |
| `UCASE()` | ❌ | Synonym for UPPER() |  |
| `UNCOMPRESS()` | ❌ | Uncompress a string compressed |  |
| `UNCOMPRESSED_LENGTH()` | ❌ | Return the length of a string before compression |  |
| `UNHEX()` | ❌ | Return a string containing hex representation of a number |  |
| `UNIX_TIMESTAMP()` | ❌ | Return a Unix timestamp |  |
| `UpdateXML()` | ❌ | Return replaced XML fragment |  |
| `UPPER()` | ❌ | Convert to uppercase |  |
| `USER()` | ❌ | The user name and host name provided by the client |  |
| `UTC_DATE()` | ❌ | Return the current UTC date |  |
| `UTC_TIME()` | ❌ | Return the current UTC time |  |
| `UTC_TIMESTAMP()` | ❌ | Return the current UTC date and time |  |
| `UUID()` | ❌ | Return a Universal Unique Identifier (UUID) |  |
| `UUID_SHORT()` | ❌ | Return an integer-valued universal identifier |  |
| `UUID_TO_BIN()` | ❌ | Convert string UUID to binary |  |
| `VALIDATE_PASSWORD_STRENGTH()` | ❌ | Determine strength of password |  |
| `VALUES()` | ❌ | Define the values to be used during an INSERT |  |
| `VAR_POP()` | ❌ | Return the population standard variance |  |
| `VAR_SAMP()` | ❌ | Return the sample variance |  |
| `VARIANCE()` | ❌ | Return the population standard variance |  |
| `VERSION()` | ❌ | Return a string that indicates the MySQL server version |  |
| `WAIT_FOR_EXECUTED_GTID_SET()` | ❌ | Wait until the given GTIDs have executed on the replica. |  |
| `WEEK()` | ❌ | Return the week number |  |
| `WEEKDAY()` | ❌ | Return the weekday index |  |
| `WEEKOFYEAR()` | ❌ | Return the calendar week of the date (1-53) |  |
| `WEIGHT_STRING()` | ❌ | Return the weight string for a string |  |
| `XOR` | ❌ | Logical XOR |  |
| `YEAR()` | ❌ | Return the year |  |
| `YEARWEEK()` | ❌ | Return the year and week |  |
| `\|` | ❌ | Bitwise OR |  |
| `~` | ❌ | Bitwise inversion |  |

## 6. Metadata Schemas

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build's availability: present objects need result-shape and privilege tests, while absent optional objects need MySQL-compatible missing-object or unavailable-plugin diagnostics.

### 6.1 INFORMATION_SCHEMA Tables

| Table | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS` | ❌ | Grantable users or roles for current user or role |  |
| `INFORMATION_SCHEMA.APPLICABLE_ROLES` | ❌ | Applicable roles for current user |  |
| `INFORMATION_SCHEMA.CHARACTER_SETS` | ❌ | Available character sets |  |
| `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` | ❌ | Table and column CHECK constraints |  |
| `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` | ❌ | Character set applicable to each collation |  |
| `INFORMATION_SCHEMA.COLLATIONS` | ❌ | Collations for each character set |  |
| `INFORMATION_SCHEMA.COLUMN_PRIVILEGES` | ❌ | Privileges defined on columns |  |
| `INFORMATION_SCHEMA.COLUMN_STATISTICS` | ❌ | Histogram statistics for column values |  |
| `INFORMATION_SCHEMA.COLUMNS` | ❌ | Columns in each table |  |
| `INFORMATION_SCHEMA.COLUMNS_EXTENSIONS` | ❌ | Column attributes for primary and secondary storage engines |  |
| `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` | ❌ | Current number of consecutive failed connection attempts per account |  |
| `INFORMATION_SCHEMA.ENABLED_ROLES` | ❌ | Roles enabled within current session |  |
| `INFORMATION_SCHEMA.ENGINES` | ❌ | Storage engine properties |  |
| `INFORMATION_SCHEMA.EVENTS` | ❌ | Event Manager events |  |
| `INFORMATION_SCHEMA.FILES` | ❌ | Files that store tablespace data |  |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` | ❌ | Pages in InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` | ❌ | LRU ordering of pages in InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS` | ❌ | InnoDB buffer pool statistics |  |
| `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES` | ❌ | Number of index pages cached per index in InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_CMP` | ❌ | Status for operations related to compressed InnoDB tables |  |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX` | ❌ | Status for operations related to compressed InnoDB tables and indexes |  |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET` | ❌ | Status for operations related to compressed InnoDB tables and indexes |  |
| `INFORMATION_SCHEMA.INNODB_CMP_RESET` | ❌ | Status for operations related to compressed InnoDB tables |  |
| `INFORMATION_SCHEMA.INNODB_CMPMEM` | ❌ | Status for compressed pages within InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_CMPMEM_RESET` | ❌ | Status for compressed pages within InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_COLUMNS` | ❌ | Columns in each InnoDB table |  |
| `INFORMATION_SCHEMA.INNODB_DATAFILES` | ❌ | Data file path information for InnoDB file-per-table and general tablespaces |  |
| `INFORMATION_SCHEMA.INNODB_FIELDS` | ❌ | Key columns of InnoDB indexes |  |
| `INFORMATION_SCHEMA.INNODB_FOREIGN` | ❌ | InnoDB foreign-key metadata |  |
| `INFORMATION_SCHEMA.INNODB_FOREIGN_COLS` | ❌ | InnoDB foreign-key column status information |  |
| `INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED` | ❌ | Snapshot of INNODB_FT_DELETED table |  |
| `INFORMATION_SCHEMA.INNODB_FT_CONFIG` | ❌ | Metadata for InnoDB table FULLTEXT index and associated processing |  |
| `INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD` | ❌ | Default list of stopwords for InnoDB FULLTEXT indexes |  |
| `INFORMATION_SCHEMA.INNODB_FT_DELETED` | ❌ | Rows deleted from InnoDB table FULLTEXT index |  |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE` | ❌ | Token information for newly inserted rows in InnoDB FULLTEXT index |  |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE` | ❌ | Inverted index information for processing text searches against InnoDB table FULLTEXT index |  |
| `INFORMATION_SCHEMA.INNODB_INDEXES` | ❌ | InnoDB index metadata |  |
| `INFORMATION_SCHEMA.INNODB_METRICS` | ❌ | InnoDB performance information |  |
| `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES` | ❌ | Session temporary-tablespace metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLES` | ❌ | InnoDB table metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES` | ❌ | InnoDB file-per-table, general, and undo tablespace metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` | ❌ | Brief file-per-table, general, undo, and system tablespace metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLESTATS` | ❌ | InnoDB table low-level status information |  |
| `INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO` | ❌ | Information about active user-created InnoDB temporary tables |  |
| `INFORMATION_SCHEMA.INNODB_TRX` | ❌ | Active InnoDB transaction information |  |
| `INFORMATION_SCHEMA.INNODB_VIRTUAL` | ❌ | InnoDB virtual generated column metadata |  |
| `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` | ❌ | Which key columns have constraints |  |
| `INFORMATION_SCHEMA.KEYWORDS` | ❌ | MySQL keywords |  |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS` | ❌ | Firewall in-memory data for account profiles Deprecated in MySQL 8.4. |  |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_WHITELIST` | ❌ | Firewall in-memory data for account profile allowlists Deprecated in MySQL 8.4. |  |
| `INFORMATION_SCHEMA.ndb_transid_mysql_connection_map` | ❌ | NDB Cluster transaction mapping table when the NDB information-schema plugin is available; unavailable in standard MySQL 8.4 Server. | Conditional NDB Cluster surface. |
| `INFORMATION_SCHEMA.OPTIMIZER_TRACE` | ❌ | Information produced by optimizer trace activity |  |
| `INFORMATION_SCHEMA.PARAMETERS` | ❌ | Stored routine parameters and stored function return values |  |
| `INFORMATION_SCHEMA.PARTITIONS` | ❌ | Table partition information |  |
| `INFORMATION_SCHEMA.PLUGINS` | ❌ | Plugin information |  |
| `INFORMATION_SCHEMA.PROCESSLIST` | ❌ | Information about currently executing threads |  |
| `INFORMATION_SCHEMA.PROFILING` | ❌ | Statement profiling information |  |
| `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` | ❌ | Foreign key information |  |
| `INFORMATION_SCHEMA.RESOURCE_GROUPS` | ❌ | Resource group information |  |
| `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS` | ❌ | Column privileges for roles available to or granted by currently enabled roles |  |
| `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS` | ❌ | Routine privileges for roles available to or granted by currently enabled roles |  |
| `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS` | ❌ | Table privileges for roles available to or granted by currently enabled roles |  |
| `INFORMATION_SCHEMA.ROUTINES` | ❌ | Stored routine information |  |
| `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES` | ❌ | Privileges defined on schemas |  |
| `INFORMATION_SCHEMA.SCHEMATA` | ❌ | Schema information |  |
| `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` | ❌ | Schema options |  |
| `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` | ❌ | Columns in each table that store spatial data |  |
| `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` | ❌ | Available spatial reference systems |  |
| `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` | ❌ | Acceptable units for ST_Distance() |  |
| `INFORMATION_SCHEMA.STATISTICS` | ❌ | Table index statistics |  |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` | ❌ | Which tables have constraints |  |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` | ❌ | Table constraint attributes for primary and secondary storage engines |  |
| `INFORMATION_SCHEMA.TABLE_PRIVILEGES` | ❌ | Privileges defined on tables |  |
| `INFORMATION_SCHEMA.TABLES` | ❌ | Table information |  |
| `INFORMATION_SCHEMA.TABLES_EXTENSIONS` | ❌ | Table attributes for primary and secondary storage engines |  |
| `INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS` | ❌ | Tablespace attributes for primary storage engines |  |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATE` | ❌ | Thread pool thread group states |  |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS` | ❌ | Thread pool thread group statistics |  |
| `INFORMATION_SCHEMA.TP_THREAD_STATE` | ❌ | Thread pool thread information |  |
| `INFORMATION_SCHEMA.TRIGGERS` | ❌ | Trigger information |  |
| `INFORMATION_SCHEMA.USER_ATTRIBUTES` | ❌ | User comments and attributes |  |
| `INFORMATION_SCHEMA.USER_PRIVILEGES` | ❌ | Privileges defined globally per user |  |
| `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` | ❌ | Stored functions used in views |  |
| `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` | ❌ | Tables and views used in views |  |
| `INFORMATION_SCHEMA.VIEWS` | ❌ | View information |  |

### 6.2 Performance Schema Tables

| Table | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `performance_schema.accounts` | ❌ | Connection statistics per client account |  |
| `performance_schema.binary_log_transaction_compression_stats` | ❌ | Binary log transaction compression |  |
| `performance_schema.clone_progress` | ❌ | Clone operation progress |  |
| `performance_schema.clone_status` | ❌ | Clone operation status |  |
| `performance_schema.component_scheduler_tasks` | ❌ | Status of scheduled tasks |  |
| `performance_schema.cond_instances` | ❌ | Synchronization object instances |  |
| `performance_schema.data_lock_waits` | ❌ | Data lock wait relationships |  |
| `performance_schema.data_locks` | ❌ | Data locks held and requested |  |
| `performance_schema.error_log` | ❌ | Server error log recent entries |  |
| `performance_schema.events_errors_summary_by_account_by_error` | ❌ | Errors per account and error code |  |
| `performance_schema.events_errors_summary_by_host_by_error` | ❌ | Errors per host and error code |  |
| `performance_schema.events_errors_summary_by_thread_by_error` | ❌ | Errors per thread and error code |  |
| `performance_schema.events_errors_summary_by_user_by_error` | ❌ | Errors per user and error code |  |
| `performance_schema.events_errors_summary_global_by_error` | ❌ | Errors per error code |  |
| `performance_schema.events_stages_current` | ❌ | Current stage events |  |
| `performance_schema.events_stages_history` | ❌ | Most recent stage events per thread |  |
| `performance_schema.events_stages_history_long` | ❌ | Most recent stage events overall |  |
| `performance_schema.events_stages_summary_by_account_by_event_name` | ❌ | Stage events per account and event name |  |
| `performance_schema.events_stages_summary_by_host_by_event_name` | ❌ | Stage events per host name and event name |  |
| `performance_schema.events_stages_summary_by_thread_by_event_name` | ❌ | Stage waits per thread and event name |  |
| `performance_schema.events_stages_summary_by_user_by_event_name` | ❌ | Stage events per user name and event name |  |
| `performance_schema.events_stages_summary_global_by_event_name` | ❌ | Stage waits per event name |  |
| `performance_schema.events_statements_current` | ❌ | Current statement events |  |
| `performance_schema.events_statements_histogram_by_digest` | ❌ | Statement histograms per schema and digest value |  |
| `performance_schema.events_statements_histogram_global` | ❌ | Statement histogram summarized globally |  |
| `performance_schema.events_statements_history` | ❌ | Most recent statement events per thread |  |
| `performance_schema.events_statements_history_long` | ❌ | Most recent statement events overall |  |
| `performance_schema.events_statements_summary_by_account_by_event_name` | ❌ | Statement events per account and event name |  |
| `performance_schema.events_statements_summary_by_digest` | ❌ | Statement events per schema and digest value |  |
| `performance_schema.events_statements_summary_by_host_by_event_name` | ❌ | Statement events per host name and event name |  |
| `performance_schema.events_statements_summary_by_program` | ❌ | Statement events per stored program |  |
| `performance_schema.events_statements_summary_by_thread_by_event_name` | ❌ | Statement events per thread and event name |  |
| `performance_schema.events_statements_summary_by_user_by_event_name` | ❌ | Statement events per user name and event name |  |
| `performance_schema.events_statements_summary_global_by_event_name` | ❌ | Statement events per event name |  |
| `performance_schema.events_transactions_current` | ❌ | Current transaction events |  |
| `performance_schema.events_transactions_history` | ❌ | Most recent transaction events per thread |  |
| `performance_schema.events_transactions_history_long` | ❌ | Most recent transaction events overall |  |
| `performance_schema.events_transactions_summary_by_account_by_event_name` | ❌ | Transaction events per account and event name |  |
| `performance_schema.events_transactions_summary_by_host_by_event_name` | ❌ | Transaction events per host name and event name |  |
| `performance_schema.events_transactions_summary_by_thread_by_event_name` | ❌ | Transaction events per thread and event name |  |
| `performance_schema.events_transactions_summary_by_user_by_event_name` | ❌ | Transaction events per user name and event name |  |
| `performance_schema.events_transactions_summary_global_by_event_name` | ❌ | Transaction events per event name |  |
| `performance_schema.events_waits_current` | ❌ | Current wait events |  |
| `performance_schema.events_waits_history` | ❌ | Most recent wait events per thread |  |
| `performance_schema.events_waits_history_long` | ❌ | Most recent wait events overall |  |
| `performance_schema.events_waits_summary_by_account_by_event_name` | ❌ | Wait events per account and event name |  |
| `performance_schema.events_waits_summary_by_host_by_event_name` | ❌ | Wait events per host name and event name |  |
| `performance_schema.events_waits_summary_by_instance` | ❌ | Wait events per instance |  |
| `performance_schema.events_waits_summary_by_thread_by_event_name` | ❌ | Wait events per thread and event name |  |
| `performance_schema.events_waits_summary_by_user_by_event_name` | ❌ | Wait events per user name and event name |  |
| `performance_schema.events_waits_summary_global_by_event_name` | ❌ | Wait events per event name |  |
| `performance_schema.file_instances` | ❌ | File instances |  |
| `performance_schema.file_summary_by_event_name` | ❌ | File events per event name |  |
| `performance_schema.file_summary_by_instance` | ❌ | File events per file instance |  |
| `performance_schema.firewall_group_allowlist` | ❌ | Firewall in-memory data for group profile allowlists |  |
| `performance_schema.firewall_groups` | ❌ | Firewall in-memory data for group profiles |  |
| `performance_schema.firewall_membership` | ❌ | Firewall in-memory data for group profile members |  |
| `performance_schema.global_status` | ❌ | Global status variables |  |
| `performance_schema.global_variables` | ❌ | Global system variables |  |
| `performance_schema.host_cache` | ❌ | Information from internal host cache |  |
| `performance_schema.hosts` | ❌ | Connection statistics per client host name |  |
| `performance_schema.keyring_component_status` | ❌ | Status information for installed keyring component |  |
| `performance_schema.keyring_keys` | ❌ | Metadata for keyring keys |  |
| `performance_schema.log_status` | ❌ | Information about server logs for backup purposes |  |
| `performance_schema.memory_summary_by_account_by_event_name` | ❌ | Memory operations per account and event name |  |
| `performance_schema.memory_summary_by_host_by_event_name` | ❌ | Memory operations per host and event name |  |
| `performance_schema.memory_summary_by_thread_by_event_name` | ❌ | Memory operations per thread and event name |  |
| `performance_schema.memory_summary_by_user_by_event_name` | ❌ | Memory operations per user and event name |  |
| `performance_schema.memory_summary_global_by_event_name` | ❌ | Memory operations globally per event name |  |
| `performance_schema.metadata_locks` | ❌ | Metadata locks and lock requests |  |
| `performance_schema.mutex_instances` | ❌ | Mutex synchronization object instances |  |
| `performance_schema.ndb_sync_excluded_objects` | ❌ | NDB objects which cannot be synchronized |  |
| `performance_schema.ndb_sync_pending_objects` | ❌ | NDB objects waiting for synchronization |  |
| `performance_schema.objects_summary_global_by_type` | ❌ | Object summaries |  |
| `performance_schema.performance_timers` | ❌ | Which event timers are available |  |
| `performance_schema.persisted_variables` | ❌ | Contents of mysqld-auto.cnf file |  |
| `performance_schema.prepared_statements_instances` | ❌ | Prepared statement instances and statistics |  |
| `performance_schema.processlist` | ❌ | Process list information |  |
| `performance_schema.replication_applier_configuration` | ❌ | Configuration parameters for replication applier on replica |  |
| `performance_schema.replication_applier_filters` | ❌ | Channel-specific replication filters on current replica |  |
| `performance_schema.replication_applier_global_filters` | ❌ | Global replication filters on current replica |  |
| `performance_schema.replication_applier_status` | ❌ | Current status of replication applier on replica |  |
| `performance_schema.replication_applier_status_by_coordinator` | ❌ | SQL or coordinator thread applier status |  |
| `performance_schema.replication_applier_status_by_worker` | ❌ | Worker thread applier status |  |
| `performance_schema.replication_asynchronous_connection_failover` | ❌ | Source lists for asynchronous connection failover mechanism |  |
| `performance_schema.replication_asynchronous_connection_failover_managed` | ❌ | Managed source lists for asynchronous connection failover mechanism |  |
| `performance_schema.replication_connection_configuration` | ❌ | Configuration parameters for connecting to source |  |
| `performance_schema.replication_connection_status` | ❌ | Current status of connection to source |  |
| `performance_schema.replication_group_communication_information` | ❌ | Replication group configuration options |  |
| `performance_schema.replication_group_configuration_version` | ❌ | Version of the member actions configuration for replication group members |  |
| `performance_schema.replication_group_member_actions` | ❌ | Member actions that are included in the member actions configuration for replication group members |  |
| `performance_schema.replication_group_member_stats` | ❌ | Replication group member statistics |  |
| `performance_schema.replication_group_members` | ❌ | Replication group member network and status |  |
| `performance_schema.rwlock_instances` | ❌ | Lock synchronization object instances |  |
| `performance_schema.session_account_connect_attrs` | ❌ | Connection attributes per for current session |  |
| `performance_schema.session_connect_attrs` | ❌ | Connection attributes for all sessions |  |
| `performance_schema.session_status` | ❌ | Status variables for current session |  |
| `performance_schema.session_variables` | ❌ | System variables for current session |  |
| `performance_schema.setup_actors` | ❌ | How to initialize monitoring for new foreground threads |  |
| `performance_schema.setup_consumers` | ❌ | Consumers for which event information can be stored |  |
| `performance_schema.setup_instruments` | ❌ | Classes of instrumented objects for which events can be collected |  |
| `performance_schema.setup_objects` | ❌ | Which objects should be monitored |  |
| `performance_schema.setup_threads` | ❌ | Instrumented thread names and attributes |  |
| `performance_schema.socket_instances` | ❌ | Active connection instances |  |
| `performance_schema.socket_summary_by_event_name` | ❌ | Socket waits and I/O per event name |  |
| `performance_schema.socket_summary_by_instance` | ❌ | Socket waits and I/O per instance |  |
| `performance_schema.status_by_account` | ❌ | Session status variables per account |  |
| `performance_schema.status_by_host` | ❌ | Session status variables per host name |  |
| `performance_schema.status_by_thread` | ❌ | Session status variables per session |  |
| `performance_schema.status_by_user` | ❌ | Session status variables per user name |  |
| `performance_schema.table_handles` | ❌ | Table locks and lock requests |  |
| `performance_schema.table_io_waits_summary_by_index_usage` | ❌ | Table I/O waits per index |  |
| `performance_schema.table_io_waits_summary_by_table` | ❌ | Table I/O waits per table |  |
| `performance_schema.table_lock_waits_summary_by_table` | ❌ | Table lock waits per table |  |
| `performance_schema.threads` | ❌ | Information about server threads |  |
| `performance_schema.tls_channel_status` | ❌ | TLS status for each connection interface |  |
| `performance_schema.tp_thread_group_state` | ❌ | Thread pool thread group states |  |
| `performance_schema.tp_thread_group_stats` | ❌ | Thread pool thread group statistics |  |
| `performance_schema.tp_thread_state` | ❌ | Thread pool thread information |  |
| `performance_schema.user_defined_functions` | ❌ | Registered loadable functions |  |
| `performance_schema.user_variables_by_thread` | ❌ | User-defined variables per thread |  |
| `performance_schema.users` | ❌ | Connection statistics per client user name |  |
| `performance_schema.variables_by_thread` | ❌ | Session system variables per session |  |
| `performance_schema.variables_info` | ❌ | How system variables were most recently set |  |
| `performance_schema.tp_connections` | ❌ | Thread pool connection state and queue information. |  |

### 6.3 sys Schema Objects

| Object | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `sys.sys_config` | ❌ | Expose MySQL-compatible sys schema table behavior, result shape, and diagnostics. |  |
| `sys.sys_config_insert_set_user` | ❌ | Expose MySQL-compatible sys schema trigger behavior, result shape, and diagnostics. |  |
| `sys.sys_config_update_set_user` | ❌ | Expose MySQL-compatible sys schema trigger behavior, result shape, and diagnostics. |  |
| `sys.host_summary` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_file_io` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_file_io` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_file_io_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_file_io_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_stages` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_stages` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_statement_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_statement_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_statement_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_statement_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.innodb_buffer_stats_by_schema` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$innodb_buffer_stats_by_schema` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.innodb_buffer_stats_by_table` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$innodb_buffer_stats_by_table` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.innodb_lock_waits` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$innodb_lock_waits` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_by_thread_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_by_thread_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_file_by_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_file_by_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_file_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_file_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_wait_by_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_wait_by_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_wait_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_wait_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.latest_file_io` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$latest_file_io` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_by_host_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_by_host_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_by_thread_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_by_thread_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_by_user_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_by_user_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_global_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_global_by_current_bytes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_global_total` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_global_total` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.metrics` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.processlist` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$processlist` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$ps_digest_95th_percentile_by_avg_us` | ❌ | Expose MySQL-compatible sys schema helper view behavior, result shape, and diagnostics. | Helper view for statement runtime percentile reporting. |
| `sys.x$ps_digest_avg_latency_distribution` | ❌ | Expose MySQL-compatible sys schema helper view behavior, result shape, and diagnostics. | Helper view for statement runtime percentile reporting. |
| `sys.x$ps_schema_table_statistics_io` | ❌ | Expose MySQL-compatible sys schema helper view behavior, result shape, and diagnostics. | Helper view for schema table statistics. |
| `sys.ps_check_lost_instrumentation` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_auto_increment_columns` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_index_statistics` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_index_statistics` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_object_overview` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_redundant_indexes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_flattened_keys` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_table_lock_waits` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_table_lock_waits` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_table_statistics` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_table_statistics` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_table_statistics_with_buffer` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_table_statistics_with_buffer` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_tables_with_full_table_scans` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_tables_with_full_table_scans` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_unused_indexes` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.session` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$session` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.session_ssl_status` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statement_analysis` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statement_analysis` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_errors_or_warnings` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_errors_or_warnings` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_full_table_scans` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_full_table_scans` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_runtimes_in_95th_percentile` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_runtimes_in_95th_percentile` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_sorting` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_sorting` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_temp_tables` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_temp_tables` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_file_io` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_file_io` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_file_io_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_file_io_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_stages` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_stages` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_statement_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_statement_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_statement_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_statement_type` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.version` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.wait_classes_global_by_avg_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$wait_classes_global_by_avg_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.wait_classes_global_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$wait_classes_global_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.waits_by_host_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$waits_by_host_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.waits_by_user_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$waits_by_user_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.waits_global_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$waits_global_by_latency` | ❌ | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.create_synonym_db()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.diagnostics()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.execute_prepared_stmt()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_background_threads()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_consumer()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_instrument()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_thread()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_background_threads()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_consumer()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_instrument()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_thread()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_reload_saved()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_reset_to_default()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_save()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_disabled()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_disabled_consumers()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_disabled_instruments()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_enabled()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_enabled_consumers()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_enabled_instruments()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_statement_avg_latency_histogram()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_trace_statement_digest()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_trace_thread()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_truncate_all_tables()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.statement_performance_analyzer()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.table_exists()` | ❌ | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.extract_schema_from_file_name()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.extract_table_from_file_name()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_bytes()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_path()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_statement()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_time()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.list_add()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.list_drop()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_account_enabled()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_consumer_enabled()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_instrument_default_enabled()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_instrument_default_timed()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_thread_instrumented()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_account()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_id()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_stack()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_trx_info()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.quote_identifier()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.sys_get_config()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.version_major()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.version_minor()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.version_patch()` | ❌ | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |

### 6.4 mysql System Schema and Data Dictionary

| Table | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `mysql.catalogs` | ❌ | Invisible data dictionary table for catalog metadata. |  |
| `mysql.character_sets` | ❌ | Invisible data dictionary table for character set metadata. |  |
| `mysql.check_constraints` | ❌ | Invisible data dictionary table for CHECK constraint metadata. |  |
| `mysql.collations` | ❌ | Invisible data dictionary table for collation metadata. |  |
| `mysql.column_statistics` | ❌ | Invisible data dictionary table for histogram statistics. |  |
| `mysql.column_type_elements` | ❌ | Invisible data dictionary table for ENUM/SET and other column type elements. |  |
| `mysql.columns` | ❌ | Invisible data dictionary table for table column metadata. |  |
| `mysql.dd_properties` | ❌ | Invisible data dictionary table for dictionary version and upgrade metadata. |  |
| `mysql.events` | ❌ | Invisible data dictionary table for Event Scheduler metadata. |  |
| `mysql.foreign_keys` | ❌ | Invisible data dictionary table for foreign key metadata. |  |
| `mysql.foreign_key_column_usage` | ❌ | Invisible data dictionary table for foreign key column mappings. |  |
| `mysql.index_column_usage` | ❌ | Invisible data dictionary table for index column usage. |  |
| `mysql.index_partitions` | ❌ | Invisible data dictionary table for index partition metadata. |  |
| `mysql.index_stats` | ❌ | Invisible data dictionary table for dynamic index statistics. |  |
| `mysql.indexes` | ❌ | Invisible data dictionary table for index metadata. |  |
| `mysql.innodb_ddl_log` | ❌ | Invisible data dictionary table for crash-safe DDL logs. |  |
| `mysql.parameter_type_elements` | ❌ | Invisible data dictionary table for routine parameter and return type elements. |  |
| `mysql.parameters` | ❌ | Invisible data dictionary table for stored routine parameters and function return values. |  |
| `mysql.resource_groups` | ❌ | Invisible data dictionary table for resource group metadata. |  |
| `mysql.routines` | ❌ | Invisible data dictionary table for stored procedure and function metadata. |  |
| `mysql.schemata` | ❌ | Invisible data dictionary table for schema metadata. |  |
| `mysql.st_spatial_reference_systems` | ❌ | Invisible data dictionary table for spatial reference systems. |  |
| `mysql.table_partition_values` | ❌ | Invisible data dictionary table for partition values. |  |
| `mysql.table_partitions` | ❌ | Invisible data dictionary table for table partition metadata. |  |
| `mysql.table_stats` | ❌ | Invisible data dictionary table for dynamic table statistics. |  |
| `mysql.tables` | ❌ | Invisible data dictionary table for table metadata. |  |
| `mysql.tablespace_files` | ❌ | Invisible data dictionary table for tablespace files. |  |
| `mysql.tablespaces` | ❌ | Invisible data dictionary table for active tablespaces. |  |
| `mysql.triggers` | ❌ | Invisible data dictionary table for trigger metadata. |  |
| `mysql.view_routine_usage` | ❌ | Invisible data dictionary table for view-to-routine dependencies. |  |
| `mysql.view_table_usage` | ❌ | Invisible data dictionary table for view-to-table dependencies. |  |
| `mysql.user` | ❌ | Grant table for accounts, global privileges, authentication, and nonprivilege account attributes. |  |
| `mysql.global_grants` | ❌ | Grant table for dynamic global privilege assignments. |  |
| `mysql.db` | ❌ | Grant table for database-level privileges. |  |
| `mysql.tables_priv` | ❌ | Grant table for table-level privileges. |  |
| `mysql.columns_priv` | ❌ | Grant table for column-level privileges. |  |
| `mysql.procs_priv` | ❌ | Grant table for routine privileges. |  |
| `mysql.proxies_priv` | ❌ | Grant table for proxy-user privileges. |  |
| `mysql.default_roles` | ❌ | Grant table for default role activation. |  |
| `mysql.role_edges` | ❌ | Grant table for role graph edges. |  |
| `mysql.password_history` | ❌ | Grant table for password history. |  |
| `mysql.component` | ❌ | Registry for server components installed with INSTALL COMPONENT. |  |
| `mysql.func` | ❌ | Registry for loadable functions installed with CREATE FUNCTION. |  |
| `mysql.plugin` | ❌ | Registry for server-side plugins installed with INSTALL PLUGIN. |  |
| `mysql.general_log` | ❌ | CSV log table for the general query log. |  |
| `mysql.slow_log` | ❌ | CSV log table for the slow query log. |  |
| `mysql.help_category` | ❌ | Server-side HELP category table. |  |
| `mysql.help_keyword` | ❌ | Server-side HELP keyword table. |  |
| `mysql.help_relation` | ❌ | Server-side HELP relation table. |  |
| `mysql.help_topic` | ❌ | Server-side HELP topic table. |  |
| `mysql.time_zone` | ❌ | Time zone ID and leap-second usage table. |  |
| `mysql.time_zone_leap_second` | ❌ | Leap-second transition table. |  |
| `mysql.time_zone_name` | ❌ | Time zone name mapping table. |  |
| `mysql.time_zone_transition` | ❌ | Time zone transition table. |  |
| `mysql.time_zone_transition_type` | ❌ | Time zone transition type table. |  |
| `mysql.gtid_executed` | ❌ | Replication table storing GTID values. |  |
| `mysql.ndb_binlog_index` | ❌ | NDB Cluster replication binary log information table. |  |
| `mysql.slave_master_info` | ❌ | Replication metadata repository table for source connection metadata. |  |
| `mysql.slave_relay_log_info` | ❌ | Replication metadata repository table for relay log metadata. |  |
| `mysql.slave_worker_info` | ❌ | Replication metadata repository table for worker metadata. |  |
| `mysql.innodb_index_stats` | ❌ | Optimizer table for InnoDB persistent index statistics. |  |
| `mysql.innodb_table_stats` | ❌ | Optimizer table for InnoDB persistent table statistics. |  |
| `mysql.server_cost` | ❌ | Optimizer cost model table for general server operation costs. |  |
| `mysql.engine_cost` | ❌ | Optimizer cost model table for storage-engine operation costs. |  |
| `mysql.audit_log_filter` | ❌ | Enterprise Audit table for persistent audit filter definitions. |  |
| `mysql.audit_log_user` | ❌ | Enterprise Audit table for persistent audit user mappings. |  |
| `mysql.firewall_group_allowlist` | ❌ | Enterprise Firewall table for group profile allowlists. |  |
| `mysql.firewall_groups` | ❌ | Enterprise Firewall table for group profiles. |  |
| `mysql.firewall_membership` | ❌ | Enterprise Firewall table for group profile memberships. |  |
| `mysql.firewall_users` | ❌ | Enterprise Firewall table for account profiles. |  |
| `mysql.firewall_whitelist` | ❌ | Deprecated Enterprise Firewall allowlist table. |  |
| `mysql.servers` | ❌ | FEDERATED storage engine server definition table. |  |
| `mysql.innodb_dynamic_metadata` | ❌ | InnoDB table for fast-changing table metadata such as auto-increment counters. |  |

## 7. Runtime Configuration, Modes, and Variables

### 7.1 Session and Runtime State

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| Default schema | ❌ | DATABASE()/SCHEMA(), USE, schema-qualified names, and single-file mapping. |  |
| Connection character set state | ❌ | character_set_client, character_set_connection, character_set_results, collation_connection, and SET NAMES behavior. |  |
| Time zone state | ❌ | global/session time_zone, system_time_zone, temporal functions, TIMESTAMP conversion, and named time zones. |  |
| Autocommit state | ❌ | autocommit behavior, implicit transactions, and transaction boundary metadata. |  |
| Last insert id | ❌ | LAST_INSERT_ID(), OK packet insert id, explicit LAST_INSERT_ID(expr), and multi-row behavior. |  |
| Affected rows | ❌ | CLIENT_FOUND_ROWS, changed rows vs matched rows, DDL/DML OK packets, and warning counts. |  |
| Warnings and diagnostics | ❌ | SHOW WARNINGS, SHOW ERRORS, GET DIAGNOSTICS, warning count, sql_notes, and truncation warnings. |  |
| Prepared statement registry | ❌ | Per-connection prepared statement namespace and deallocation behavior. |  |
| Temporary table namespace | ❌ | Per-session temporary table metadata and shadowing. |  |
| Role and privilege state | ❌ | CURRENT_ROLE(), CURRENT_USER(), DEFINER/INVOKER, and privilege-check-visible metadata. |  |
| Locks | ❌ | Named locks, table locks, metadata locks, backup locks, and lock diagnostics. |  |

### 7.2 SQL Modes

| SQL mode | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `ALLOW_INVALID_DATES` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ANSI` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ANSI_QUOTES` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ERROR_FOR_DIVISION_BY_ZERO` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `HIGH_NOT_PRECEDENCE` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `IGNORE_SPACE` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_AUTO_VALUE_ON_ZERO` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_BACKSLASH_ESCAPES` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_DIR_IN_CREATE` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_ENGINE_SUBSTITUTION` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_UNSIGNED_SUBTRACTION` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_ZERO_DATE` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_ZERO_IN_DATE` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ONLY_FULL_GROUP_BY` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `PAD_CHAR_TO_FULL_LENGTH` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `PIPES_AS_CONCAT` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `REAL_AS_FLOAT` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `STRICT_ALL_TABLES` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `STRICT_TRANS_TABLES` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `TIME_TRUNCATE_FRACTIONAL` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `TRADITIONAL` | ❌ | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |

### 7.3 Server System Variables

The exact value, scope, mutability, privilege requirement, persisted-variable behavior, optional plugin/build availability, and `SHOW VARIABLES`/`performance_schema` exposure must be verified per variable.

| Variable | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `activate_all_roles_on_login` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_address` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_port` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_ca` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_capath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_cert` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_cipher` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_crl` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_crlpath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_tls_ciphersuites` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_tls_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_compression` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_connection_policy` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_current_session` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_database` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_disable` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_encryption` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_exclude_accounts` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_filter_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_flush` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_flush_interval_seconds` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_format` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_format_unix_timestamp` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_include_accounts` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_password_history_keep_days` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_policy` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_prune_seconds` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_read_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_rotate_on_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_statement_policy` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_strategy` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_kerberos_service_key_tab` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_kerberos_service_principal` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_auth_method_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_bind_base_dn` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_bind_root_dn` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_bind_root_pwd` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_ca_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_connect_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_group_search_attr` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_group_search_filter` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_init_pool_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_log_status` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_max_pool_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_referral` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_response_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_server_host` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_server_port` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_tls` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_user_search_attr` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_auth_method_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_bind_base_dn` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_bind_root_dn` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_bind_root_pwd` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_ca_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_connect_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_group_search_attr` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_group_search_filter` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_init_pool_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_log_status` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_max_pool_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_referral` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_response_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_server_host` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_server_port` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_tls` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_user_search_attr` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_policy` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_webauthn_rp_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_windows_log_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_windows_use_principal_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `auto_generate_certs` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `auto_increment_increment` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `auto_increment_offset` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `autocommit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `automatic_sp_privileges` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `back_log` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `basedir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `big_tables` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `bind_address` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_checksum` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_direct_non_transactional_updates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_encryption` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_error_action` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_expire_logs_auto_purge` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_expire_logs_seconds` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_format` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_group_commit_sync_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_group_commit_sync_no_delay_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_gtid_simple_recovery` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_max_flush_queue_time` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_order_commits` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_rotate_encryption_master_key_at_startup` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_event_max_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_image` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_metadata` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_value_options` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_rows_query_log_events` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_stmt_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_transaction_compression` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_transaction_compression_level_zstd` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_transaction_dependency_history_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `block_encryption_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `build_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `bulk_insert_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_auto_generate_rsa_keys` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_digest_rounds` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_private_key_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_public_key_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_client` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_connection` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_database` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_filesystem` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_results` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_server` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_system` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_sets_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `check_proxy_users` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_autotune_concurrency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_block_ddl` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ddl_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_delay_after_data_drop` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_donor_timeout_after_network_failure` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_enable_compression` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_max_concurrency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_max_data_bandwidth` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_max_network_bandwidth` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ssl_ca` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ssl_cert` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ssl_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_valid_donor_list` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `collation_connection` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `collation_database` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `collation_server` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `completion_type` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `component_masking.dictionaries_flush_interval_seconds` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `component_masking.masking_database` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `component_scheduler.enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `concurrent_insert` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connect_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_control_failed_connections_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_control_max_connection_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_control_min_connection_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_memory_chunk_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_memory_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `core_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `create_admin_listener_thread` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `cte_max_recursion_depth` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `datadir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `debug_sync` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_collation_for_utf8mb4` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_password_lifetime` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_storage_engine` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_table_encryption` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_tmp_storage_engine` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_week_format` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delay_key_write` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delayed_insert_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delayed_insert_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delayed_queue_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `disabled_storage_engines` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `disconnect_on_expired_password` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `div_precision_increment` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `dragnet.log_error_filter_rules` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `end_markers_in_json` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `enforce_gtid_consistency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `enterprise_encryption.maximum_rsa_key_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `enterprise_encryption.rsa_support_legacy_padding` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `eq_range_index_dive_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `error_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `event_scheduler` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `explain_format` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `explain_json_format_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `explicit_defaults_for_timestamp` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `external_user` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `flush` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `flush_time` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `foreign_key_checks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_boolean_syntax` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_max_word_len` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_min_word_len` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_query_expansion_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_stopword_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `general_log` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `general_log_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `generated_random_password_length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `global_connection_memory_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `global_connection_memory_tracking` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_concat_max_len` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_advertise_recovery_endpoints` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_allow_local_lower_version_join` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_auto_increment_increment` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_autorejoin_tries` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_bootstrap_group` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_clone_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_communication_debug_options` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_communication_max_message_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_communication_stack` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_components_stop_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_compression_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_consistency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_enforce_update_everywhere_checks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_exit_state_action` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_applier_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_certifier_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_hold_percent` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_max_quota` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_member_quota_percent` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_min_quota` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_min_recovery_quota` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_period` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_release_percent` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_force_members` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_group_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_group_seeds` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_gtid_assignment_block_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_ip_allowlist` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_local_address` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_member_expel_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_member_weight` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_message_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_paxos_single_leader` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_poll_spin_loops` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_preemptive_garbage_collection` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_preemptive_garbage_collection_rows_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_compression_algorithms` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_get_public_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_public_key_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_reconnect_interval` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_retry_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_ca` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_capath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_cert` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_cipher` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_crl` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_crlpath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_verify_server_cert` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_tls_ciphersuites` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_tls_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_use_ssl` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_zstd_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_single_primary_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_ssl_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_start_on_boot` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_tls_source` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_transaction_size_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_unreachable_majority_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_view_change_uuid` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_executed` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_executed_compression_period` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_next` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_owned` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_purged` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_compress` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_dynamic_loading` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_geometry` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_profiling` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_query_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_rtree_keys` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_statement_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_symlink` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `histogram_generation_max_mem_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `host_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `hostname` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `identity` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `immediate_server_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `information_schema_stats_expiry` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_connect` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_replica` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_slave` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_flushing` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_flushing_lwm` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_hash_index` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_hash_index_parts` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_max_sleep_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_autoextend_increment` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_autoinc_lock_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_background_drop_list_empty` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_chunk_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_dump_at_shutdown` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_dump_now` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_dump_pct` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_filename` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_in_core_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_load_abort` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_load_at_startup` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_load_now` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_change_buffer_max_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_change_buffering` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_change_buffering_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_checkpoint_disabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_checksum_algorithm` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_cmp_per_index_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_commit_concurrency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compress_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compression_failure_threshold_pct` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compression_pad_pct_max` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_concurrency_tickets` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_data_file_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_data_home_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ddl_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ddl_log_crash_reset_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ddl_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_deadlock_detect` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_dedicated_server` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_default_row_format` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_directories` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_disable_sort_file_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_batch_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_files` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_pages` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_extend_and_initialize` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fast_shutdown` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fil_make_page_dirty_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_file_per_table` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fill_factor` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_log_at_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_log_at_trx_commit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_method` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_neighbors` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_sync` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flushing_avg_loops` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_force_load_corrupted` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_force_recovery` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fsync_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_aux_table` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_enable_diag_print` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_enable_stopword` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_max_token_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_min_token_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_num_word_optimize` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_result_cache_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_server_stopword_table` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_sort_pll_degree` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_total_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_user_stopword_table` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_idle_flush_pct` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_io_capacity` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_io_capacity_max` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_limit_optimistic_insert_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_lock_wait_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_checkpoint_fuzzy_now` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_checkpoint_now` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_checksums` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_compressed_pages` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_file_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_files_in_group` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_group_home_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_spin_cpu_abs_lwm` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_spin_cpu_pct_hwm` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_wait_for_flush_spin_hwm` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_write_ahead_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_writer_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_lru_scan_depth` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_dirty_pages_pct` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_dirty_pages_pct_lwm` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_purge_lag` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_purge_lag_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_undo_log_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_merge_threshold_set_all_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_disable` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_enable` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_reset` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_reset_all` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_numa_interleave` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_old_blocks_pct` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_old_blocks_time` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_online_alter_log_max_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_open_files` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_optimize_fulltext_only` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_page_cleaners` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_page_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_parallel_read_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_print_all_deadlocks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_print_ddl_logs` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_purge_batch_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_purge_rseg_truncate_frequency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_purge_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_random_read_ahead` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_read_ahead_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_read_io_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_read_only` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_redo_log_archive_dirs` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_redo_log_capacity` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_redo_log_encrypt` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_replication_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_rollback_on_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_rollback_segments` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_saved_page_number_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_segment_reserve_factor` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sort_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_spin_wait_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_spin_wait_pause_multiplier` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_auto_recalc` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_include_delete_marked` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_method` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_on_metadata` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_persistent` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_persistent_sample_pages` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_transient_sample_pages` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_status_output` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_status_output_locks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_strict_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sync_array_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sync_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sync_spin_loops` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_table_locks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_temp_data_file_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_temp_tablespaces_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_thread_concurrency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_thread_sleep_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_tmpdir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_trx_purge_view_update_only_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_trx_rseg_n_slots_debug` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_directory` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_log_encrypt` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_log_truncate` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_tablespaces` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_use_fdatasync` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_use_native_aio` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_validate_tablespace_paths` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_write_io_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `insert_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `interactive_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `internal_tmp_mem_storage_engine` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `join_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keep_files_on_create` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_cache_age_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_cache_block_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_cache_division_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_cmk_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_conf_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_data_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_region` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_auth_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_ca_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_caching` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_auth_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_ca_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_caching` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_role_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_server_url` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_store_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_role_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_secret_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_server_url` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_store_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_okv_conf_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_operations` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `large_files_support` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `large_page_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `large_pages` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `last_insert_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lc_messages` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lc_messages_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lc_time_names` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `license` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `local_infile` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_loop` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_missing_arc` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_missing_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_missing_unlock` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_dependencies` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_extra_dependencies` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_output_directory` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_print_txt` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_loop` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_missing_arc` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_missing_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_missing_unlock` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_wait_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `locked_in_memory` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin_basename` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin_index` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin_trust_function_creators` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error_services` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error_suppression_list` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error_verbosity` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_output` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_queries_not_using_indexes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_raw` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_replica_updates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slave_updates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_admin_statements` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_extra` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_replica_statements` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_slave_statements` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_statements_unsafe_for_binlog` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_throttle_queries_not_using_indexes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_timestamps` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `long_query_time` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `low_priority_updates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lower_case_file_system` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lower_case_table_names` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mandatory_roles` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `master_verify_checksum` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_allowed_packet` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_binlog_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_binlog_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_binlog_stmt_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_connect_errors` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_connections` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_delayed_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_digest_length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_error_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_execution_time` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_heap_table_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_insert_delayed_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_join_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_length_for_sort_data` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_points_in_geometry` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_prepared_stmt_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_relay_log_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_seeks_for_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_sort_length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_sp_recursion_depth` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_user_connections` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_write_lock_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mecab_rc_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `min_examined_row_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_data_pointer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_max_sort_file_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_mmap_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_recover_options` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_sort_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_stats_method` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_use_mmap` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_database` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_reload_interval_seconds` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_trace` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_native_password_proxy_users` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_bind_address` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_compression_algorithms` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_connect_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_deflate_default_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_deflate_max_client_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_document_id_unique_prefix` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_enable_hello_notice` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_idle_worker_thread_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_interactive_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_lz4_default_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_lz4_max_client_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_max_allowed_packet` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_max_connections` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_min_worker_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_port` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_port_open_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_read_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_socket` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_ca` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_capath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_cert` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_cipher` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_crl` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_crlpath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_wait_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_write_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_zstd_default_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_zstd_max_client_compression_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `named_pipe` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `named_pipe_full_access_group` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_allow_copying_alter_table` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_applier_allow_skip_epoch` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_autoincrement_prefetch_sz` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_batch_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_blob_read_batch_bytes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_blob_write_batch_bytes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_clear_apply_status` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_cluster_connection_pool` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_cluster_connection_pool_nodeids` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_conflict_role` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_data_node_neighbour` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_dbg_check_shares` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_default_column_format` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_deferred_constraints` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_distribution` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_eventbuffer_free_percent` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_eventbuffer_max_alloc` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_extra_logging` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_force_send` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_fully_replicated` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_index_stat_enable` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_index_stat_option` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_join_pushdown` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_apply_status` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_bin` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_binlog_index` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_empty_epochs` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_empty_update` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_exclusive_reads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_fail_terminate` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_orig` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_compression` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_compression_level_zstd` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_dependency` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_update_as_write` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_update_minimal` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_updated_only` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_metadata_check` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_metadata_check_interval` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_metadata_sync` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_mgm_tls` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_optimization_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_optimized_node_selection` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_read_backup` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_recv_thread_activation_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_recv_thread_cpu_mask` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_replica_batch_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_replica_blob_write_batch_bytes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `Ndb_replica_max_replicated_epoch` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_report_thresh_binlog_epoch_slip` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_report_thresh_binlog_mem_usage` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_row_checksum` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_schema_dist_lock_wait_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_schema_dist_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_schema_dist_upgrade_allowed` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `Ndb_schema_participant_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_show_foreign_key_mock_tables` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_slave_conflict_role` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `Ndb_system_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_table_no_logging` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_table_temporary` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_tls_search_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_use_copying_alter_table` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_use_exact_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_use_transactions` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_version_string` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_wait_connected` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_wait_setup` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_database` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_max_bytes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_max_rows` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_offline` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_show_hidden` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_table_prefix` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_buffer_length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_read_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_retry_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_write_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ngram_token_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `offline_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `old_alter_table` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `open_files_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_prune_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_search_depth` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_switch` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_features` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_max_mem_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_offset` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `original_commit_timestamp` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `original_server_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `parser_max_mem_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `partial_revokes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `password_history` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `password_require_current` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `password_reuse_interval` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_accounts_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_digests_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_error_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_stages_history_long_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_stages_history_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_statements_history_long_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_statements_history_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_transactions_history_long_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_transactions_history_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_waits_history_long_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_waits_history_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_hosts_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_cond_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_cond_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_digest_length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_digest_sample_age` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_file_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_file_handles` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_file_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_index_stat` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_memory_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_metadata_locks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_meter_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_metric_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_mutex_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_mutex_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_prepared_statements_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_program_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_rwlock_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_rwlock_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_socket_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_socket_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_sql_text_length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_stage_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_statement_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_statement_stack` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_table_handles` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_table_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_table_lock_stat` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_thread_classes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_thread_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_session_connect_attrs_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_setup_actors_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_setup_objects_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_show_processlist` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_users_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `persist_only_admin_x509_subject` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `persist_sensitive_variables_in_plaintext` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `persisted_globals_load` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pid_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `plugin_dir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `port` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `preload_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `print_identified_with_as_hex` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `profiling` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `profiling_history_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `protocol_compression_algorithms` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `protocol_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `proxy_user` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pseudo_replica_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pseudo_slave_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pseudo_thread_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `query_alloc_block_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `query_prealloc_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rand_seed1` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rand_seed2` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `range_alloc_block_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `range_optimizer_max_mem_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rbr_exec_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `read_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `read_only` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `read_rnd_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `regexp_stack_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `regexp_time_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_basename` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_index` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_purge` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_recovery` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_space_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_allow_batching` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_checkpoint_group` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_checkpoint_period` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_compressed_protocol` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_exec_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_load_tmpdir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_max_allowed_packet` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_net_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_parallel_type` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_parallel_workers` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_pending_jobs_size_max` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_preserve_commit_order` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_skip_errors` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_sql_verify_checksum` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_transaction_retries` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_type_conversions` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replication_optimize_for_static_plugin_config` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replication_sender_observe_commit_only` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_host` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_password` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_port` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_user` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `require_row_format` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `require_secure_transport` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `restrict_fk_on_non_standard_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `resultset_metadata` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rewriter_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rewriter_enabled_for_threads_without_privilege_checks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rewriter_verbose` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_read_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_trace_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_wait_for_slave_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_wait_no_slave` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_wait_point` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_replica_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_replica_trace_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_slave_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_slave_trace_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_trace_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_wait_for_replica_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_wait_no_replica` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_wait_point` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_stop_replica_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_stop_slave_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `schema_definition_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `secondary_engine_cost_threshold` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `secure_file_priv` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `select_into_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `select_into_disk_sync` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `select_into_disk_sync_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `server_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `server_id_bits` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `server_uuid` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_gtids` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_schema` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_state_change` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_system_variables` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_transaction_info` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `set_operations_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_auto_generate_rsa_keys` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_private_key_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_proxy_users` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_public_key_path` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `shared_memory` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `shared_memory_base_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `show_create_table_skip_secondary_engine` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `show_create_table_verbosity` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `show_gipk_in_create_table_and_information_schema` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_external_locking` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_name_resolve` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_networking` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_replica_start` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_show_database` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_slave_start` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_allow_batching` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_checkpoint_group` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_checkpoint_period` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_compressed_protocol` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_exec_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_load_tmpdir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_max_allowed_packet` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_net_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_parallel_type` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_parallel_workers` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_pending_jobs_size_max` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_preserve_commit_order` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_skip_errors` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_sql_verify_checksum` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_transaction_retries` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_type_conversions` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slow_launch_time` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slow_query_log` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slow_query_log_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `socket` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sort_buffer_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `source_verify_checksum` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_auto_is_null` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_big_selects` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_buffer_result` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_generate_invisible_primary_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_log_bin` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_log_off` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_notes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_quote_show_create` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_replica_skip_counter` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_require_primary_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_safe_updates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_select_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_slave_skip_counter` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_warnings` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_ca` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_capath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_cert` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_cipher` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_crl` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_crlpath` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_fips_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_session_cache_mode` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_session_cache_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `statement_id` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `stored_program_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `stored_program_definition_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `super_read_only` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_binlog` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_master_info` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_relay_log` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_relay_log_info` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_source_info` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `syseventlog.facility` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `syseventlog.include_pid` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `syseventlog.tag` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `system_time_zone` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_definition_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_encryption_privilege_check` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_open_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_open_cache_instances` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tablespace_definition_cache` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_reader_frequency_1` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_reader_frequency_2` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_reader_frequency_3` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_bsp_max_export_batch_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_bsp_max_queue_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_bsp_schedule_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_certificates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_cipher` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_cipher_suite` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_client_certificates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_client_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_compression` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_endpoint` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_headers` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_max_tls` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_min_tls` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_protocol` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_certificates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_cipher` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_cipher_suite` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_client_certificates` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_client_key` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_compression` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_endpoint` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_headers` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_max_tls` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_min_tls` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_protocol` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_log_level` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_resource_attributes` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.query_text_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.trace_enabled` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `temptable_max_mmap` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `temptable_max_ram` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `temptable_use_mmap` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `terminology_use_previous` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_cache_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_handling` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_algorithm` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_dedicated_listeners` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_high_priority_connection` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_longrun_trx_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_max_active_query_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_max_transactions_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_max_unused_threads` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_prio_kickup_timer` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_query_threads_per_group` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_stall_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_transaction_delay` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_stack` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `time_zone` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `timestamp` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tls_certificates_enforced_validation` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tls_ciphersuites` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tls_version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tmp_table_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tmpdir` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_alloc_block_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_allow_batching` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_isolation` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_prealloc_size` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_read_only` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `unique_checks` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `updatable_views_with_limit` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `use_secondary_engine` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_check_user_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_dictionary_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_mixed_case_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_number_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_policy` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_special_char_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.changed_characters_percentage` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.check_user_name` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.dictionary_file` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.length` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.mixed_case_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.number_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.policy` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.special_char_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_comment` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_compile_machine` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_compile_os` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_compile_zlib` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_tokens_session` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_tokens_session_number` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `wait_timeout` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `warning_count` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `windowing_use_high_precision` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `xa_detach_on_prepare` | ❌ | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |

### 7.4 Server Status Variables

The exact value shape, counter lifetime, session/global visibility, optional plugin/build availability, and `SHOW STATUS`/`performance_schema` exposure must be verified per variable.

| Variable | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `Aborted_clients` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Aborted_connects` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Acl_cache_items_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_current_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_direct_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_event_max_drop_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events_filtered` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events_written` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_total_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_write_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Authentication_ldap_sasl_supported_methods` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_cache_disk_use` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_cache_use` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_stmt_cache_disk_use` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_stmt_cache_use` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Bytes_received` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Bytes_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Caching_sha2_password_rsa_public_key` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_admin_commands` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_db` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_event` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_function` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_procedure` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_resource_group` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_server` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_table` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_tablespace` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_user` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_user_default_role` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_analyze` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_assign_to_keycache` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_begin` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_binlog` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_call_procedure` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_change_db` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_change_repl_filter` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_change_replication_source` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_check` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_checksum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_clone` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_commit` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_db` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_event` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_function` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_index` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_procedure` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_resource_group` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_role` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_server` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_table` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_trigger` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_udf` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_user` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_view` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_dealloc_sql` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_delete` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_delete_multi` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_do` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_db` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_event` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_function` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_index` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_procedure` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_resource_group` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_role` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_server` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_table` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_trigger` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_user` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_view` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_empty_query` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_execute_sql` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_explain_other` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_flush` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_get_diagnostics` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_grant` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_grant_roles` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_group_replication_start` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_group_replication_stop` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_ha_close` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_ha_open` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_ha_read` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_help` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_insert` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_insert_select` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_install_component` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_install_plugin` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_kill` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_load` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_lock_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_optimize` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_preload_keys` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_prepare_sql` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_purge` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_purge_before_date` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_release_savepoint` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rename_table` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rename_user` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_repair` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replace` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replace_select` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replica_start` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replica_stop` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_reset` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_resignal` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_restart` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_revoke` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_revoke_all` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_revoke_roles` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rollback` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rollback_to_savepoint` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_savepoint` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_select` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_set_option` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_set_resource_group` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_set_role` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_authors` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_binary_log_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_binlog_events` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_binlogs` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_charsets` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_collations` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_contributors` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_db` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_event` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_func` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_proc` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_table` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_trigger` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_user` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_databases` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_engine_logs` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_engine_mutex` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_engine_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_errors` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_events` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_fields` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_function_code` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_function_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_grants` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_keys` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_ndb_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_open_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_plugins` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_privileges` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_procedure_code` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_procedure_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_processlist` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_profile` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_profiles` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_relaylog_events` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_replica_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_replicas` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_storage_engines` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_table_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_triggers` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_variables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_warnings` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_shutdown` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_signal` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_close` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_execute` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_fetch` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_prepare` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_reprepare` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_reset` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_send_long_data` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_truncate` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_uninstall_component` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_uninstall_plugin` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_unlock_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_update` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_update_multi` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_commit` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_end` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_prepare` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_recover` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_rollback` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_start` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Compression` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Compression_algorithm` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Compression_level` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_control_delay_generated` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_accept` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_internal` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_max_connections` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_peer_address` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_select` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_tcpwrap` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connections` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Created_tmp_disk_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Created_tmp_files` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Created_tmp_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_ca` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_capath` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_cert` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_cipher` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_ciphersuites` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_crl` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_crlpath` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_key` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_version` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Delayed_errors` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Delayed_insert_threads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Delayed_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Deprecated_use_i_s_processlist_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Deprecated_use_i_s_processlist_last_timestamp` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `dragnet.Status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_buffered_bytes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_buffered_events` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_expired_events` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_latest_write` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_access_denied` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_access_granted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_access_suspicious` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_cached_entries` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Flush_commands` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Global_connection_memory` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_all_consensus_proposals_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_all_consensus_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_certification_garbage_collector_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_certification_garbage_collector_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_consensus_bytes_received_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_consensus_bytes_sent_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_control_messages_sent_bytes_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_control_messages_sent_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_control_messages_sent_roundtrip_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_data_messages_sent_bytes_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_data_messages_sent_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_data_messages_sent_roundtrip_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_empty_consensus_proposals_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_extended_consensus_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_active_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_last_throttle_timestamp` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_last_consensus_end_timestamp` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_total_messages_sent_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_sync_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_sync_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_termination_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_termination_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_before_begin_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_before_begin_time_sum` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_commit` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_delete` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_discover` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_external_lock` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_mrr_init` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_prepare` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_first` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_key` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_last` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_next` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_prev` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_rnd` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_rnd_next` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_rollback` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_savepoint` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_savepoint_rollback` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_update` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_write` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_bytes_data` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_bytes_dirty` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_dump_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_load_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_data` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_dirty` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_flushed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_free` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_latched` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_misc` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_total` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_ahead` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_ahead_evicted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_ahead_rnd` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_requests` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_reads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_resize_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_resize_status_code` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_resize_status_progress` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_wait_free` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_write_requests` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_fsyncs` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_pending_fsyncs` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_pending_reads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_pending_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_read` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_reads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_written` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_dblwr_pages_written` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_dblwr_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_have_atomic_builtins` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_log_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_log_write_requests` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_log_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_num_open_files` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_fsyncs` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_pending_fsyncs` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_pending_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_written` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_page_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_pages_created` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_pages_read` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_pages_written` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_capacity_resized` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_checkpoint_lsn` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_current_lsn` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_enabled` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_flushed_to_disk_lsn` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_logical_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_physical_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_read_only` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_resize_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_uuid` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_current_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_time_avg` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_time_max` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_deleted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_inserted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_read` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_updated` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_deleted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_inserted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_read` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_updated` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_truncated_status_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_active` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_explicit` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_implicit` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_total` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_blocks_not_flushed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_blocks_unused` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_blocks_used` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_read_requests` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_reads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_write_requests` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_writes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Last_query_cost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Last_query_partial_plans` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Locked_connects` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_execution_time_exceeded` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_execution_time_set` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_execution_time_set_failed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_used_connections` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_used_connections_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `mecab_charset` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_aborted_clients` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_address` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_received` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_received_compressed_payload` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_received_uncompressed_frame` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_sent_compressed_payload` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_sent_uncompressed_frame` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_compression_algorithm` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_compression_level` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connection_accept_errors` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connection_errors` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connections_accepted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connections_closed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connections_rejected` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_create_view` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_delete` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_drop_view` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_find` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_insert` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_modify_view` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_update` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_cursor_close` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_cursor_fetch` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_cursor_open` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_errors_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_errors_unknown_message_type` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_expect_close` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_expect_open` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_init_error` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_messages_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notice_global_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notice_other_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notice_warning_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notified_by_group_replication` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_port` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_prep_deallocate` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_prep_execute` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_prep_prepare` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_rows_sent` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_accepted` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_closed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_fatal_error` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_killed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_rejected` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_socket` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_accept_renegotiates` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_accepts` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_active` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_cipher` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_cipher_list` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_ctx_verify_depth` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_ctx_verify_mode` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_finished_accepts` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_server_not_after` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_server_not_before` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_verify_depth` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_verify_mode` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_version` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_create_collection` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_create_collection_index` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_disable_notices` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_drop_collection` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_drop_collection_index` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_enable_notices` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_ensure_collection` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_execute_mysqlx` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_execute_sql` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_execute_xplugin` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_get_collection_options` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_kill_client` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_list_clients` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_list_notices` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_list_objects` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_modify_collection_options` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_ping` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_worker_threads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_worker_threads_active` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_bytes_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_bytes_count_injector` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_data_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_data_count_injector` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_nondata_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_nondata_count_injector` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count_replica` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count_slave` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_cluster_node_id` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_config_from_host` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_config_from_port` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_config_generation` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch_trans` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch2` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch2_trans` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max_del_win` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max_del_win_ins` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max_ins` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_old` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_last_conflict_epoch` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_last_stable_epoch` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_reflected_op_discard_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_reflected_op_prepare_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_refresh_op_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_conflict_commit_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_detect_iter_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_reject_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_row_conflict_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_row_reject_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_epoch_delete_delete_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_execute_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_fetch_table_stats` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_last_commit_epoch_server` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_last_commit_epoch_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_metadata_detected_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_metadata_excluded_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_metadata_synced_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_number_of_data_nodes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pruned_scan_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_queries_defined` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_queries_dropped` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_queries_executed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_reads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_scan_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_slave_max_replicated_epoch` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_trans_hint_count_session` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Not_flushed_delayed_rows` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ongoing_anonymous_gtid_violating_transaction_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ongoing_anonymous_transaction_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ongoing_automatic_gtid_violating_transaction_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_files` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_streams` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_table_definitions` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Opened_files` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Opened_table_definitions` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Opened_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_accounts_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_cond_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_cond_instances_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_digest_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_file_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_file_handles_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_file_instances_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_hosts_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_index_stat_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_locker_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_memory_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_metadata_lock_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_meter_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_metric_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_mutex_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_mutex_instances_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_nested_statement_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_prepared_statements_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_program_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_rwlock_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_rwlock_instances_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_session_connect_attrs_longest_seen` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_session_connect_attrs_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_socket_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_socket_instances_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_stage_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_statement_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_table_handles_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_table_instances_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_table_lock_stat_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_thread_classes_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_thread_instances_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_users_lost` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Prepared_stmt_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Queries` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Questions` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Replica_open_temp_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Resource_group_supported` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_number_loaded_rules` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_number_reloads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_number_rewritten_queries` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_reload_error` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_clients` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_net_avg_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_net_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_net_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_no_times` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_no_tx` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_timefunc_failures` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_tx_avg_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_tx_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_tx_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_wait_pos_backtraverse` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_wait_sessions` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_yes_tx` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_replica_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_slave_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_clients` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_net_avg_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_net_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_net_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_no_times` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_no_tx` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_timefunc_failures` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_tx_avg_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_tx_wait_time` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_tx_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_wait_pos_backtraverse` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_wait_sessions` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_yes_tx` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rsa_public_key` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Secondary_engine_execution_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_full_join` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_full_range_join` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_range` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_range_check` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_scan` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slave_open_temp_tables` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slave_rows_last_search_algorithm_used` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slow_launch_threads` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slow_queries` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_merge_passes` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_range` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_rows` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_scan` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_accept_renegotiates` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_accepts` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_callback_cache_hits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_cipher` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_cipher_list` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_client_connects` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_connect_renegotiates` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_ctx_verify_depth` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_ctx_verify_mode` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_default_timeout` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_finished_accepts` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_finished_connects` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_server_not_after` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_server_not_before` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_hits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_misses` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_mode` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_overflows` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_timeout` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_timeouts` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_sessions_reused` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_used_session_cache_entries` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_verify_depth` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_verify_mode` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_version` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_locks_immediate` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_locks_waited` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_open_cache_hits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_open_cache_misses` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_open_cache_overflows` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tc_log_max_pages_used` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tc_log_page_size` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tc_log_page_waits` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Telemetry_metrics_supported` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Telemetry_traces_supported` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `telemetry.live_sessions` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_cached` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_connected` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_created` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_running` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tls_library_version` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tls_sni_server_name` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Uptime` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Uptime_since_flush_status` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password_dictionary_file_last_parsed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password_dictionary_file_words_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password.dictionary_file_last_parsed` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password.dictionary_file_words_count` | ❌ | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |

## 8. Wire Protocol and Client API Surface

### 8.1 Connection, Authentication, and Packets

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| Protocol 10 initial handshake | ❌ | Server greeting, connection id, auth plugin data, status flags, charset, and capability negotiation. |  |
| Capability flags | ❌ | Client/server capability negotiation and rejection semantics. |  |
| SSLRequest and TLS upgrade | ❌ | TLS negotiation, failure modes, and capability interactions. |  |
| Handshake Response 41 | ❌ | Username, auth response, database, auth plugin name, connection attributes, and charset. |  |
| `caching_sha2_password` | ❌ | Default MySQL 8 authentication plugin packet flow and RSA/TLS behavior. |  |
| `sha256_password` | ❌ | Password exchange packet flow and diagnostics. |  |
| `mysql_native_password` | ❌ | Deprecated plugin compatibility and MySQL 8.4 behavior. |  |
| Auth switch request/response | ❌ | Plugin switching packet flow. |  |
| Auth more data | ❌ | Multi-step authentication packet flow. |  |
| OK packet | ❌ | Affected rows, last insert id, status flags, warnings, session-state tracking, and EOF deprecation. |  |
| ERR packet | ❌ | Error code, SQLSTATE, message text, and fatal/nonfatal behavior. |  |
| EOF packet compatibility | ❌ | Legacy EOF packet behavior when CLIENT_DEPRECATE_EOF is not set. |  |
| Result set metadata | ❌ | Column count, column definitions, flags, charsets, decimals, schema/table/origin names, and EOF/OK termination. |  |
| Text result rows | ❌ | Text protocol row encoding, NULLs, character sets, and length-encoded values. |  |
| Binary result rows | ❌ | Prepared statement row encoding, null bitmap, type encodings, and unsigned flags. |  |
| LOCAL INFILE request | ❌ | Client file upload request flow and security controls. |  |
| Compression protocol | ❌ | Compressed packet framing and capability negotiation. |  |
| Zstandard compression | ❌ | zstd compression negotiation and packet behavior. |  |
| Connection attributes | ❌ | Attribute parsing and exposure in performance_schema/session tables. |  |
| Session state tracking | ❌ | Schema, system variables, GTIDs, transaction state, and state-change notices. |  |

### 8.2 Command Packets

| Command | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| `COM_SLEEP` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_QUIT` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_INIT_DB` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_QUERY` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_FIELD_LIST` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CREATE_DB` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_DROP_DB` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_UNUSED_2` | ❌ | Preserve MySQL 8.4.9 command-number handling for the removed `COM_REFRESH` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_UNUSED_1` | ❌ | Preserve MySQL 8.4.9 command-number handling for the removed `COM_SHUTDOWN` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_STATISTICS` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_UNUSED_4` | ❌ | Preserve MySQL 8.4.9 command-number handling for the removed `COM_PROCESS_INFO` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_CONNECT` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_UNUSED_5` | ❌ | Preserve MySQL 8.4.9 command-number handling for the removed `COM_PROCESS_KILL` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_DEBUG` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_PING` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_TIME` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_DELAYED_INSERT` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CHANGE_USER` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_BINLOG_DUMP` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_TABLE_DUMP` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CONNECT_OUT` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_REGISTER_SLAVE` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_PREPARE` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_EXECUTE` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_SEND_LONG_DATA` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_CLOSE` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_RESET` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_SET_OPTION` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_FETCH` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_DAEMON` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_BINLOG_DUMP_GTID` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_RESET_CONNECTION` | ❌ | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CLONE` | ❌ | Implement MySQL-compatible clone command packet parsing, plugin handoff behavior, status flags, and errors. |  |
| `COM_SUBSCRIBE_GROUP_REPLICATION_STREAM` | ❌ | Implement MySQL-compatible Group Replication stream subscription command, privilege checks, handoff behavior, and errors. |  |

## 9. Error, Warning, and Result Semantics

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| Error code catalog | ❌ | Use MySQL 8.4 error numbers, SQLSTATE values, and message text where practical. |  |
| Warning code catalog | ❌ | Use MySQL 8.4 warning numbers, SQLSTATE values, message text, and ordering. |  |
| Diagnostics area | ❌ | Statement diagnostics, condition areas, row count, and GET DIAGNOSTICS integration. |  |
| Strict-mode errors | ❌ | Escalation from warnings to errors under strict SQL modes. |  |
| IGNORE warning demotion | ❌ | DML IGNORE conversion, duplicate, constraint, and truncation behavior. |  |
| Metadata flags | ❌ | Column flags for NOT NULL, PRI/UNI/MUL, BLOB, UNSIGNED, ZEROFILL, BINARY, ENUM, AUTO_INCREMENT, TIMESTAMP, SET, and NUM. |  |
| Result ordering guarantees | ❌ | Match MySQL ordering only where SQL semantics or MySQL behavior require it. |  |
| Floating-point edge cases | ❌ | NaN/Inf handling, division, rounding, comparison, and platform-sensitive behavior. |  |
| Temporal edge cases | ❌ | Zero dates, leap days, DST, fractional truncation/rounding, and timezone table behavior. |  |
| JSON edge cases | ❌ | Duplicate keys, ordering, binary JSON storage-visible behavior, path errors, and partial update metadata. |  |
| Spatial edge cases | ❌ | SRID mismatch, geographic vs Cartesian calculations, invalid geometry handling, and units. |  |
| Privilege-sensitive metadata | ❌ | Metadata visibility affected by grants and DEFINER/INVOKER context. |  |

## 10. Embedded-Design Compatibility Decisions

Some MySQL server features do not naturally map to an in-process single-file database. They still belong in the compatibility matrix because applications may issue the syntax or inspect related metadata. Each row needs an explicit design decision before it can move out of `❌`.

| Feature | Status | Target behavior | Implementation notes |
| --- | --- | --- | --- |
| Replication and binary logs | ❌ | Decide between unsupported errors, no-op placeholders, metadata stubs, or local change streams. |  |
| Account management and privileges | ❌ | Decide how much auth/privilege syntax is accepted and how metadata is represented without a server account model. |  |
| Resource groups | ❌ | Decide embedded diagnostics for CPU/thread scheduling features. |  |
| Components and plugins | ❌ | Decide loadable component/plugin syntax behavior and metadata exposure. |  |
| Server lifecycle commands | ❌ | Decide diagnostics for RESTART, SHUTDOWN, CLONE, and backup-lock operations. |  |
| Storage engines | ❌ | Decide how ENGINE clauses, SHOW ENGINES, and engine-specific metadata map to SQLite-backed storage. |  |
| `InnoDB` engine surface | ❌ | Decide how MySQL's default engine metadata, syntax, and diagnostics map onto SQLite-backed storage. |  |
| `MyISAM` engine surface | ❌ | Decide parser, metadata, SHOW ENGINES, and unsupported-feature diagnostics. |  |
| `MEMORY` engine surface | ❌ | Decide temporary/in-memory table compatibility and diagnostics. |  |
| `CSV` engine surface | ❌ | Decide log-table and ENGINE=CSV compatibility behavior. |  |
| `ARCHIVE` engine surface | ❌ | Decide archive-engine syntax and diagnostics. |  |
| `BLACKHOLE` engine surface | ❌ | Decide blackhole-engine syntax and embedded-compatible behavior. |  |
| `MERGE` engine surface | ❌ | Decide MERGE/MyISAM union table syntax and diagnostics. |  |
| `FEDERATED` engine surface | ❌ | Decide foreign-server table syntax, mysql.servers metadata, and diagnostics. |  |
| `NDB` engine surface | ❌ | Decide NDB-specific DDL syntax, metadata tables, and diagnostics. |  |
| `PERFORMANCE_SCHEMA` engine surface | ❌ | Decide metadata and diagnostics for performance_schema engine tables. |  |
| Tablespaces and logfile groups | ❌ | Decide syntax acceptance and diagnostics for non-single-file storage constructs. |  |
| Performance Schema | ❌ | Decide which tables expose real embedded runtime metrics versus documented empty placeholders. |  |
| sys schema | ❌ | Decide which objects are useful over MyLite metadata and which should be documented as empty or unsupported. |  |
| File import/export | ❌ | Decide secure embedded behavior for LOAD DATA, LOAD XML, SELECT INTO OUTFILE, and SELECT INTO DUMPFILE. |  |
| User-defined/loadable functions | ❌ | Decide C-extension registration model, security boundaries, and binary size policy. |  |
| X Protocol and Document Store | ❌ | Decide whether MySQL X Protocol and CRUD-style document APIs are out of scope, stubbed, or implemented. |  |

## 11. Test Expectations

- Every implemented row needs MySQL 8.4.9 comparison tests.
- Tests must cover normal results, errors, warnings, metadata, affected rows, inserted ids, session state, and side effects.
- Syntax-only compatibility must still test parser acceptance and the exact warning/error/placeholder behavior.
- Feature work should add focused guide documentation when behavior is subtle enough that future maintainers need the design rationale.
- When MySQL behavior is platform-dependent or storage-engine-dependent, tests should document the chosen MyLite contract and the observed MySQL baseline.
