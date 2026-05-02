# MyLite ↔ MySQL 8.4.9 compatibility

This document is the initial compatibility target inventory for MyLite against MySQL 8 LTS, currently MySQL 8.4.9. It replaces the seed matrix with a MyLite-specific catalog of SQL grammar, runtime semantics, metadata, protocol, and operational surfaces that must be tracked as implementation progresses.

## Legend

| Mark | Meaning |
| :-: | --- |
| ✅ | Supported with MySQL 8.4.9-compatible behavior and runtime comparison tests for relevant results, errors, warnings, metadata, affected rows, side effects, and protocol details. |
| 🟡 | Implemented with documented compatibility gaps, reduced fidelity, or MyLite-specific behavior that is covered by tests. |
| ⚪ | Accepted at parse or API level and intentionally handled as an embedded-compatible no-op, placeholder, warning, or diagnostic. |
| ❌ | Not implemented, rejected, or not yet MySQL-runtime verified in MyLite. |

## Prioritization rubric

Priority is a delivery-order signal, not a relaxation of correctness:

- `top`: Blocks WordPress Core installation, ordinary page requests, core schema upgrades, common `wpdb` flows, MySQL client interoperability, or the highest-install WordPress.org plugins.
- `high`: Common in popular plugins, ecommerce/forms/security/migration tooling, admin diagnostics, schema migration paths, or mainstream MySQL application code.
- `medium`: Useful compatibility for less common plugin features, advanced SQL, metadata inspection, or broader ecosystem coverage that does not usually block WordPress-first adoption.
- `low`: Exotic, server-administration-only, storage-engine-specific, replication/cluster-only, deprecated, debug-only, or rarely used compatibility surface.

## Source inventory

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
- [WordPress.org popular plugins](https://wordpress.org/plugins/browse/popular/)
- [WordPress `wpdb` class reference](https://developer.wordpress.org/reference/classes/wpdb/)
- [WordPress Plugin Handbook: Creating Tables with Plugins](https://developer.wordpress.org/plugins/creating-tables-with-plugins/)

## Maintenance rules

- Keep this file as an implementation contract, not a marketing scorecard.
- Split rows when syntax, behavior, metadata, warnings, or side effects need independent tests.
- Document deliberate embedded-design incompatibilities explicitly instead of silently dropping them.
- Keep `COMPATIBILITY.md`, feature guides, design notes, and MySQL-runtime tests in sync.
- Prefer MySQL 8.4.9 behavior over convenience, SQLite defaults, or older MySQL/MariaDB behavior.

## 1. SQL statement surface

The parser should eventually recognize the full MySQL grammar. Unsupported embedded-server features may still be accepted with a MySQL-compatible diagnostic, warning, or placeholder when that is safer for applications than a syntax error.

### 1.1 Data definition statements

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `ALTER DATABASE` / `ALTER SCHEMA` | 🟡 | high | Database default character set, collation, encryption, and read-only options. | Implemented for the MyLite schema catalog, including omitted-name default-schema targeting and validation for the initial `utf8mb4`, `utf8mb3`, `latin1`, and `binary` charset/collation registry; full read-only enforcement, privileges, and warning records are deferred. See [schema lifecycle spec](docs/specs/schema-lifecycle/specs.md) and [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `ALTER EVENT` | ❌ | medium | Event scheduler metadata and body changes. |  |
| `ALTER FUNCTION` | ❌ | medium | Stored-function metadata characteristics. |  |
| `ALTER INSTANCE` | ❌ | low | Instance reload, TLS, keyring, and master-key operations with embedded-compatible behavior. |  |
| `ALTER LOGFILE GROUP` | ❌ | low | NDB logfile group syntax and diagnostics. |  |
| `ALTER PROCEDURE` | ❌ | medium | Stored-procedure metadata characteristics. |  |
| `ALTER SERVER` | ❌ | low | Foreign server metadata changes. |  |
| `ALTER TABLE` | ❌ | top | Full table rebuild/in-place/instant surface; see section 3.2. |  |
| `ALTER TABLESPACE` | ❌ | low | General tablespace alterations and diagnostics. |  |
| `ALTER UNDO TABLESPACE` | ❌ | low | Undo tablespace syntax from the MySQL parser. |  |
| `ALTER VIEW` | ❌ | high | View replacement while preserving MySQL metadata and security semantics. |  |
| `CREATE DATABASE` / `CREATE SCHEMA` | 🟡 | high | Database creation syntax, defaults, warnings, and single-file mapping. | Implemented as catalog rows inside the single `.mylite` file, with `IF NOT EXISTS`, normalized charset/collation defaults for the initial registry, and encryption value validation; warning records and full charset/collation catalog coverage are deferred. See [schema lifecycle spec](docs/specs/schema-lifecycle/specs.md) and [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `CREATE EVENT` | ❌ | medium | Scheduled event definition, body, definer, comments, and scheduler metadata. |  |
| `CREATE FUNCTION` (stored) | ❌ | medium | Stored-function definition, determinism, SQL data access, security, and body semantics. |  |
| `CREATE FUNCTION` (loadable) | ❌ | low | Loadable-function registration syntax with embedded-compatible diagnostics. |  |
| `CREATE INDEX` | ❌ | top | Standalone index creation over MySQL index types and attributes. | Started/speced only; no parser or runtime support yet. First slice targets metadata-backed ordinary and unique standalone indexes with MySQL-compatible duplicate validation, statistics metadata, write-path conflict effects, and deferred full-text/spatial/functional surfaces. See [standalone CREATE INDEX and DROP INDEX spec](docs/specs/create-drop-index/specs.md). |
| `CREATE LOGFILE GROUP` | ❌ | low | NDB logfile group syntax and diagnostics. |  |
| `CREATE PROCEDURE` | ❌ | medium | Stored procedure parameters, body, characteristics, and diagnostics. |  |
| `CREATE SERVER` | ❌ | low | Foreign server metadata syntax. |  |
| `CREATE SPATIAL REFERENCE SYSTEM` | ❌ | medium | Spatial reference system catalog DDL. |  |
| `CREATE TABLE` | 🟡 | top | Column definitions, constraints, indexes, table options, generated columns, and partitions; see section 3.1. | Base execution is implemented for the supported table-definition subset: schema-qualified/default-schema creation, `IF NOT EXISTS` deterministic no-op, SQLite physical table creation, internal table/column/index catalog rows, common table options, primary/unique/secondary index metadata, and critical duplicate validation. Physical write-time defaults, constraints, index enforcement, warnings, generated columns, references, checks, partitions, temporary tables, `LIKE`, and CTAS are deferred. See [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| `CREATE TEMPORARY TABLE` | ❌ | high | Session-scoped table lifecycle and name shadowing. |  |
| `CREATE TABLE ... LIKE` | ❌ | high | Exact metadata cloning rules. |  |
| `CREATE TABLE ... SELECT` | ❌ | high | CTAS type inference, default handling, indexes, and atomicity. |  |
| `CREATE TABLESPACE` | ❌ | low | General and NDB tablespace syntax and diagnostics. |  |
| `CREATE UNDO TABLESPACE` | ❌ | low | Undo tablespace syntax and diagnostics. |  |
| `CREATE TRIGGER` | ❌ | high | Trigger timing, event, ordering, body, definer, and metadata. |  |
| `CREATE VIEW` | ❌ | high | View column names, algorithms, security, check options, and metadata. |  |
| `DROP DATABASE` / `DROP SCHEMA` | 🟡 | high | Schema removal, metadata cleanup, warnings, and embedded single-file constraints. | Implemented for schema catalog rows, including `IF EXISTS` and clearing the selected default schema; table cleanup, privilege cleanup, and warning records wait for later metadata work. See [schema lifecycle spec](docs/specs/schema-lifecycle/specs.md). |
| `DROP EVENT` | ❌ | medium | Event metadata deletion. |  |
| `DROP FUNCTION` (stored) | ❌ | medium | Stored-function deletion and routine metadata cleanup. |  |
| `DROP FUNCTION` (loadable) | ❌ | low | Loadable-function deregistration syntax. |  |
| `DROP INDEX` | ❌ | top | Standalone index removal semantics. | Started/speced only; no parser or runtime support yet. First slice targets `DROP INDEX name ON table` metadata cleanup, unique-conflict-surface removal, MySQL diagnostics, and deferred primary-key dependency edge cases. See [standalone CREATE INDEX and DROP INDEX spec](docs/specs/create-drop-index/specs.md). |
| `DROP LOGFILE GROUP` | ❌ | low | NDB logfile group syntax and diagnostics. |  |
| `DROP PROCEDURE` | ❌ | medium | Stored-procedure deletion and metadata cleanup. |  |
| `DROP SERVER` | ❌ | low | Foreign server metadata deletion. |  |
| `DROP SPATIAL REFERENCE SYSTEM` | ❌ | medium | Spatial reference system deletion and dependency checks. |  |
| `DROP TABLE` | 🟡 | top | Multi-table drop, temporary tables, foreign-key checks, and warnings. | Implemented for base tables created by the supported `CREATE TABLE` subset, including schema-qualified/default-schema resolution, physical SQLite table removal, table/column/index catalog cleanup, `IF EXISTS` mixed existing/missing behavior, duplicate-target validation, all-or-nothing cleanup, and no-op `RESTRICT`/`CASCADE`. Temporary-table storage, warning records, foreign-key interactions, and implicit commit semantics are deferred; `DROP TEMPORARY TABLE` never drops base tables. See [DROP TABLE base execution spec](docs/specs/drop-table/specs.md). |
| `DROP TABLESPACE` | ❌ | low | Tablespace deletion syntax and diagnostics. |  |
| `DROP UNDO TABLESPACE` | ❌ | low | Undo tablespace deletion syntax and diagnostics. |  |
| `DROP TRIGGER` | ❌ | high | Trigger deletion and metadata cleanup. |  |
| `DROP VIEW` | ❌ | high | Multi-view drop and warnings. |  |
| `RENAME TABLE` | ❌ | top | Atomic multi-table rename semantics. |  |
| `TRUNCATE TABLE` | ❌ | top | DDL-like truncate, auto-increment reset, implicit commit, and foreign-key restrictions. |  |
| Atomic DDL | ❌ | top | Atomicity and crash-safety expectations for MySQL DDL equivalents. |  |
| Implicit commit boundaries | ❌ | top | Statements that cause implicit commits before and/or after execution. | Repeated `START TRANSACTION`/`BEGIN` now commits the active explicit transaction before starting a new one. Full DDL implicit-commit retrofits remain deferred, so non-temporary DDL boundaries are still not covered. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |

### 1.2 Data manipulation statements

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `CALL` | ❌ | medium | Procedure invocation, result sets, OUT/INOUT parameters, and diagnostics. |  |
| `DELETE` (single-table) | 🟡 | top | Single-table delete with aliases, partitions, ORDER BY, LIMIT, LOW_PRIORITY, QUICK, and IGNORE. | Executable for one user base table created by the supported `CREATE TABLE` subset, including selected-schema and schema-qualified target resolution, aliases, `WHERE`, `ORDER BY`, `LIMIT row_count`, Task 25 CASE expression predicates/order keys, physical deletion, affected rows, deterministic diagnostics, rollback, default strict-mode expression warning promotion, and `AUTO_INCREMENT` sequence preservation. `LOW_PRIORITY`, `QUICK`, `IGNORE`, partitions, CTEs, multi-table forms, subqueries, broad expressions beyond the supported scalar subset, trigger/foreign-key/generated-column effects, and full SQL-mode warning-demotion behavior remain deferred. See [single-table DELETE spec](docs/specs/delete-single-table/specs.md) and [CASE expression spec](docs/specs/case-expression/specs.md). |
| `DELETE` (multi-table) | ❌ | high | Multi-table delete forms using FROM and USING, join semantics, and affected rows. |  |
| `DO` | ❌ | medium | Expression execution with warning and error semantics. |  |
| `HANDLER` | ❌ | low | HANDLER OPEN, READ, and CLOSE cursor-like table access. |  |
| `IMPORT TABLE` | ❌ | low | Transportable tablespace import syntax and diagnostics. |  |
| `INSERT ... VALUES` | 🟡 | top | Multi-row values, defaults, generated columns, warnings, affected rows, and insert ids. | Executable for base tables created by the supported `CREATE TABLE` subset, including optional `INTO`, optional column lists, `VALUE` synonym, `ROW(...)` constructors, all-default rows, schema/default-schema target resolution, duplicate/unknown column diagnostics, row-count diagnostics, `NOT NULL` default/null diagnostics, primary/unique duplicate checks, physical SQLite writes, atomic multi-row rollback, deterministic literal/default/current-timestamp values, row aliases used by the scoped ODKU surface, MySQL-style `AUTO_INCREMENT` allocation side effects, statement affected rows, and session last insert id via public API. Arbitrary row expressions, full type conversion/range warnings, arbitrary generated default-expression/function evaluation, priority/delayed/partition modifiers, and insert-from-query sources remain deferred. The first `INSERT IGNORE` and `ON DUPLICATE KEY UPDATE` slices are tracked separately below. See [INSERT ... VALUES spec](docs/specs/insert-values/specs.md). |
| `INSERT ... SET` | 🟡 | top | Assignment-form insert semantics, defaults, duplicate column diagnostics, affected rows, and insert ids. | Executable for base tables created by the supported `CREATE TABLE` subset, including optional `INTO`, schema-qualified targets, unqualified/table-qualified/schema-table-qualified assignment targets, unknown-before-duplicate target diagnostics, case-insensitive duplicate target detection, omitted/defaulted values, deterministic literals, `NULL`, `DEFAULT`, `CURRENT_TIMESTAMP`, scoped assignment-order column references and arithmetic, row aliases used by the scoped ODKU surface, `AUTO_INCREMENT` allocation, explicit high values, duplicate-key rollback with sequence consumption, affected rows, and session last insert id. Full type conversion, broad expression/function evaluation, priority/delayed modifiers, partitions, and insert-from-query sources remain deferred. The first `INSERT IGNORE` and `ON DUPLICATE KEY UPDATE` slices are tracked separately below. See [INSERT ... SET spec](docs/specs/insert-set/specs.md). |
| `INSERT ... SELECT` | ❌ | high | Insert from query expression with locking, defaults, and metadata inference. |  |
| `INSERT ... ON DUPLICATE KEY UPDATE` | 🟡 | top | Conflict target resolution, VALUES()/row aliases, affected rows, and warnings. | Implemented for the currently supported `INSERT ... VALUES`, `VALUE`, `VALUES ROW(...)`, `INSERT ... SET`, and first `INSERT IGNORE` surfaces: parser/AST support, row aliases and optional column aliases, candidate-row validation before update, catalog-order primary/unique conflict selection, source-order update assignments with repeated targets, target-row references, candidate values through row aliases/column aliases/`VALUES(col)`, 1287 warnings for `VALUES(col)`, affected rows `1`/`2`/`0`, update-branch duplicate rollback, `IGNORE` demotion/continuation for update-branch duplicate conflicts, generated auto-increment consumption, and last insert id for generated insert-path rows only. Insert-from-query sources, partitions, priority/delayed modifiers, generated columns, triggers, foreign keys, explicit `LAST_INSERT_ID(expr)`, full conversion/range/truncation demotion, and broad expression support remain deferred. See [INSERT ... ON DUPLICATE KEY UPDATE spec](docs/specs/insert-on-duplicate-key-update/specs.md). |
| `INSERT IGNORE` | 🟡 | top | Duplicate and constraint warning demotion rules for current insert forms. | First slice implemented for the currently supported `INSERT ... VALUES` and `INSERT ... SET` surfaces: parser/AST support, row-continuing primary/unique duplicate demotion, required non-auto column demotion for explicit `NULL`, explicit `DEFAULT`, and omitted no-default columns, warning records for 1062/1048/1364, affected rows for accepted rows only, and generated `AUTO_INCREMENT` consumption without using ignored rows for `last_insert_id`. The scoped ODKU surface also demotes update-branch duplicate conflicts to warning 1062 and continues. Full type conversion, range clipping, string truncation, invalid temporal demotion, priority/delayed/partition modifiers, and broader ODKU interactions remain deferred. See [INSERT IGNORE spec](docs/specs/insert-ignore/specs.md). |
| `INSERT DELAYED` | ❌ | low | Deprecated delayed insert syntax and MySQL-compatible diagnostics. |  |
| `INSERT LOW_PRIORITY` / `HIGH_PRIORITY` | ❌ | low | Priority modifiers and embedded-compatible treatment. |  |
| `LOAD DATA INFILE` | ❌ | high | Server-side text import syntax, field/line options, user variables, SET clause, warnings, and security restrictions. |  |
| `LOAD DATA LOCAL INFILE` | ❌ | high | Client-side LOCAL INFILE request flow, security controls, warnings, and protocol interaction. |  |
| `LOAD XML INFILE` | ❌ | low | Server-side XML import syntax, row matching, namespaces, SET clause, warnings, and security restrictions. |  |
| `LOAD XML LOCAL INFILE` | ❌ | low | Client-side XML import request behavior and embedded-compatible diagnostics. |  |
| `REPLACE ... VALUES` | 🟡 | top | Delete-then-insert semantics, cascades, triggers, affected rows, and auto-increment behavior. | Implemented for supported MyLite base tables with optional `INTO`, optional/empty column lists, `VALUE`, `VALUES ROW(...)`, all-default rows, schema-qualified targets, explicit primary/unique conflict scanning, delete-all-conflicting-rows behavior, source-order multi-row replacement, nullable unique parts not conflicting, affected rows as inserted plus deleted rows, statement rollback on fatal errors, and current `INSERT` auto-increment/last-insert-id semantics. Query-source replace, partitions, triggers, foreign keys, generated columns, broad conversion fidelity, and protocol OK-info formatting remain deferred. See [REPLACE spec](docs/specs/replace/specs.md). |
| `REPLACE ... SET` | 🟡 | high | SET-form replace semantics. | Implemented for supported MyLite base tables with qualified and unqualified assignment targets, candidate-row defaults/order semantics reused from `INSERT ... SET`, required-column validation before conflict deletes, explicit delete-then-insert conflict handling, affected rows, rollback, and auto-increment/last-insert-id behavior. Query-source replace and advanced table features remain deferred. See [REPLACE spec](docs/specs/replace/specs.md). |
| `REPLACE ... SELECT` | ❌ | high | Replace from query expression semantics. | Started/speced only; no parser or runtime support yet. Query-source replacement order, `TABLE` source behavior, and runtime execution remain deferred until insert-from-query support exists. See [REPLACE spec](docs/specs/replace/specs.md). |
| `REPLACE LOW_PRIORITY` / `DELAYED` | 🟡 | low | Priority and deprecated delayed modifiers for REPLACE. | Implemented for executable `VALUES` and `SET` sources: `LOW_PRIORITY` parses as an embedded no-op and `DELAYED` records warning 3005 before normal replacement. `HIGH_PRIORITY` and `IGNORE` remain syntax errors. See [REPLACE spec](docs/specs/replace/specs.md). |
| `SELECT` | 🟡 | top | Full query expression surface; see section 2. | Table-backed SELECT core is implemented for one user base table, selected-schema and schema-qualified resolution, direct column references, supported Task 16 projection expressions, Task 24 pure scalar function expressions, Task 25 CASE expressions, projection aliases, `*`, qualified wildcards, invisible-column wildcard omission, duplicate output labels, statement-owned label/schema/table/origin metadata, deterministic diagnostics, single-table `WHERE` predicate evaluation over the supported expression subset, Task 18 `ORDER BY` / `LIMIT` / `OFFSET` execution, Task 25 `COUNT`/`SUM`/`AVG`/`MIN`/`MAX` aggregate grouping, Task 26 base-table inner/cross/comma joins with `ON` and `USING`, Task 27 base-table `LEFT`/`RIGHT` outer joins with null extension, Task 28 `DISTINCT`/`DISTINCTROW` duplicate elimination over the current scalar/table-backed SELECT surfaces, Task 29 uncorrelated scalar, `EXISTS`, scalar `IN`/`NOT IN`, scalar quantified-comparison, row scalar-comparison, row `IN`/`NOT IN`, and row quantified alias subqueries in current expression contexts, and Task 23 result descriptors. No-table scalar `SELECT` expressions are implemented for the Task 16 operator subset, the Task 24 pure scalar function subset, Task 25 CASE expressions, top-level no-table aggregate calls, duplicate-mode parsing for `ALL`, and Task 29 uncorrelated scalar/`EXISTS`/scalar `IN`/`NOT IN`/scalar quantified-comparison/row scalar-comparison/row `IN`/`NOT IN`/row quantified alias subqueries. Natural joins, derived tables, correlated subqueries, general row quantified comparisons outside the accepted aliases, broader quantified subquery shapes, remaining function families, optimizer pushdown, exhaustive expression metadata, and protocol metadata remain deferred. See [table-backed SELECT core spec](docs/specs/select-table-core/specs.md), [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md), [WHERE clause spec](docs/specs/where-clause/specs.md), [ORDER BY, LIMIT, and OFFSET spec](docs/specs/order-limit-offset/specs.md), [result metadata and expression labels spec](docs/specs/result-metadata-expression-labels/specs.md), [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md), [CASE expression spec](docs/specs/case-expression/specs.md), [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md), [inner joins spec](docs/specs/inner-joins/specs.md), [outer joins spec](docs/specs/outer-joins/specs.md), [SELECT DISTINCT spec](docs/specs/select-distinct/specs.md), [subqueries spec](docs/specs/subqueries/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md), [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md), and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| `SELECT ... INTO var_list` | ❌ | high | User/local variable assignment semantics. |  |
| `SELECT ... INTO OUTFILE` | ❌ | medium | File export syntax and embedded-compatible diagnostics. |  |
| `SELECT ... INTO DUMPFILE` | ❌ | medium | Binary file export syntax and embedded-compatible diagnostics. |  |
| `TABLE` | ❌ | medium | Table-value statement syntax and ordering/limit behavior. |  |
| `UPDATE` (single-table) | 🟡 | top | Assignment order, generated columns, ORDER BY, LIMIT, LOW_PRIORITY, and IGNORE. | Executable for one user base table created by the supported `CREATE TABLE` subset, including selected-schema and schema-qualified target resolution, aliases, source-order assignments, repeated targets, `DEFAULT`, `WHERE`, `ORDER BY`, `LIMIT row_count`, Task 24 pure scalar function expressions, Task 25 CASE expression assignments/predicates/order keys, changed-row affected counts, no-op updates, deterministic diagnostics, atomic rollback, primary/unique conflict checks, order-sensitive key updates, nullable/default handling, required-column errors, and explicit `AUTO_INCREMENT` sequence advancement without changing last insert id. `LOW_PRIORITY`, `IGNORE`, partitions, generated-column execution, automatic `ON UPDATE`, broad type conversion and SQL-mode warning behavior, remaining function families, and multi-table forms remain deferred. See [single-table UPDATE spec](docs/specs/update-single-table/specs.md), [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md), and [CASE expression spec](docs/specs/case-expression/specs.md). |
| `UPDATE` (multi-table) | ❌ | high | Joined update semantics, assignment evaluation, and affected rows. |  |
| `VALUES` | ❌ | high | Standalone values statement and row constructor behavior. |  |

### 1.3 Transactional, locking, replication, prepared, and compound statements

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `START TRANSACTION` | 🟡 | top | Transaction start modifiers including READ WRITE, READ ONLY, and WITH CONSISTENT SNAPSHOT. | Parser, AST, and runtime support are implemented for `READ WRITE`, `READ ONLY`, duplicate same-kind characteristics, `WITH CONSISTENT SNAPSHOT` under the default repeatable-read compatibility state, repeated-start implicit commit, affected rows, no result columns, DML savepoint atomicity inside explicit transactions, and read-only DML rejection. Full isolation variables, `SET TRANSACTION`, and DDL implicit-commit retrofits remain deferred. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `BEGIN` / `BEGIN WORK` | 🟡 | top | Transaction begin statement distinct from compound BEGIN ... END. | Top-level `BEGIN` and `BEGIN WORK` parse and execute as transaction starters, including repeated-begin implicit commit and affected rows `0`. Stored-program `BEGIN ... END` remains deferred to the stored-program parser surface. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `COMMIT` | 🟡 | top | AND CHAIN, AND NO CHAIN, RELEASE, NO RELEASE, completion_type, and diagnostics. | Parser and runtime support are implemented for `COMMIT`, `COMMIT WORK`, no-active success, affected rows `0`, `AND CHAIN`, `AND NO CHAIN`, `RELEASE`, `NO RELEASE`, `AND CHAIN NO RELEASE`, `AND NO CHAIN RELEASE`, and `AND NO CHAIN NO RELEASE`. `completion_type` system-variable behavior and exact protocol disconnect mapping remain deferred. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `ROLLBACK` | 🟡 | top | AND CHAIN, AND NO CHAIN, RELEASE, NO RELEASE, completion_type, and diagnostics. | Parser and runtime support are implemented for `ROLLBACK`, `ROLLBACK WORK`, no-active success, affected rows `0`, `AND CHAIN`, `AND NO CHAIN`, `RELEASE`, `NO RELEASE`, `AND CHAIN NO RELEASE`, `AND NO CHAIN RELEASE`, and `AND NO CHAIN NO RELEASE`. `completion_type` system-variable behavior and exact protocol disconnect mapping remain deferred. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `SAVEPOINT` | 🟡 | top | Nested savepoint creation and replacement semantics. | Parser, AST, and runtime support are implemented for no-op behavior outside active autocommit-on transactions, case-insensitive names, same-name replacement, nested savepoints, and separation from Task 21 statement-owned savepoints. `SET autocommit`, stored-program savepoint levels, and DDL implicit-commit clearing remain deferred. See [savepoints spec](docs/specs/savepoints/specs.md). |
| `ROLLBACK TO SAVEPOINT` | 🟡 | top | Partial rollback semantics and errors. | Parser and runtime support are implemented for `ROLLBACK [WORK] TO [SAVEPOINT]`, target-retaining partial rollback, deletion of later savepoints, error 1305 for missing names, transaction-preserving errors, read-only transactions, and pending `AUTO_INCREMENT` preservation. `SET autocommit` and stored-program savepoint levels remain deferred. See [savepoints spec](docs/specs/savepoints/specs.md). |
| `RELEASE SAVEPOINT` | 🟡 | top | Savepoint release semantics and errors. | Parser and runtime support are implemented for target-and-later savepoint release without commit or rollback, error 1305 for missing names, and generated internal SQLite names. `SET autocommit`, status counters, and stored-program savepoint levels remain deferred. See [savepoints spec](docs/specs/savepoints/specs.md). |
| `SET TRANSACTION` | ❌ | high | Isolation level and access mode at global/session/next-transaction scope. |  |
| `LOCK INSTANCE FOR BACKUP` | ❌ | low | Backup lock syntax and embedded-compatible behavior. |  |
| `UNLOCK INSTANCE` | ❌ | low | Backup lock release syntax. |  |
| `LOCK TABLES` | ❌ | high | READ, READ LOCAL, WRITE, LOW_PRIORITY WRITE, aliases, and implicit commit behavior. |  |
| `UNLOCK TABLES` | ❌ | high | Table lock release and transaction interaction. |  |
| `XA START` | ❌ | low | XA transaction branch start. |  |
| `XA END` | ❌ | low | XA transaction branch end. |  |
| `XA PREPARE` | ❌ | low | XA prepare phase. |  |
| `XA COMMIT` | ❌ | low | XA one-phase and two-phase commit. |  |
| `XA ROLLBACK` | ❌ | low | XA rollback. |  |
| `XA RECOVER` | ❌ | low | XA recovery result-set metadata. |  |
| `BINLOG` | ❌ | low | Base64 binary log event statement syntax and embedded-compatible diagnostics. |  |
| `PURGE BINARY LOGS` | ❌ | low | Binary log purge syntax. |  |
| `RESET BINARY LOGS AND GTIDS` | ❌ | low | Binary log and GTID reset syntax. |  |
| `SET sql_log_bin` | ❌ | low | Session binary logging toggle and privilege semantics. |  |
| `CHANGE REPLICATION FILTER` | ❌ | low | Replication filter syntax and diagnostics. |  |
| `CHANGE REPLICATION SOURCE TO` | ❌ | low | Source connection/channel options and diagnostics. |  |
| `RESET REPLICA` | ❌ | low | Replica metadata reset syntax. |  |
| `START REPLICA` | ❌ | low | Replica start syntax, channels, threads, and until conditions. |  |
| `STOP REPLICA` | ❌ | low | Replica stop syntax and channel handling. |  |
| `START GROUP_REPLICATION` | ❌ | low | Group Replication start syntax and user credentials. |  |
| `STOP GROUP_REPLICATION` | ❌ | low | Group Replication stop syntax. |  |
| `PREPARE` | ❌ | high | Prepare from literal or user variable, parameter marker rules, and errors. |  |
| `EXECUTE` | ❌ | high | Prepared-statement execution with USING variables and result metadata. |  |
| `DEALLOCATE PREPARE` / `DROP PREPARE` | ❌ | high | Prepared statement cleanup. |  |
| `BEGIN ... END` | ❌ | medium | Compound statement block scope for stored programs and events. |  |
| Statement labels | ❌ | medium | Label declaration, LEAVE/ITERATE binding, and duplicate-label diagnostics. |  |
| `DECLARE` local variables | ❌ | medium | Stored-program local variable declarations, defaults, and scope. |  |
| `DECLARE ... CONDITION` | ❌ | medium | Named condition declarations. |  |
| `DECLARE ... CURSOR` | ❌ | medium | Cursor declaration over SELECT statements. |  |
| `DECLARE ... HANDLER` | ❌ | medium | CONTINUE/EXIT handler declarations for SQLSTATE, errors, warnings, and NOT FOUND. |  |
| `CASE` statement | ❌ | medium | Stored-program CASE statement semantics. |  |
| `IF` statement | ❌ | medium | Stored-program IF/ELSEIF/ELSE semantics. |  |
| `LOOP` | ❌ | medium | Stored-program LOOP semantics. |  |
| `REPEAT` | ❌ | medium | Stored-program REPEAT UNTIL semantics. |  |
| `WHILE` | ❌ | medium | Stored-program WHILE semantics. |  |
| `ITERATE` | ❌ | medium | Loop iteration transfer. |  |
| `LEAVE` | ❌ | medium | Block/loop exit transfer. |  |
| `RETURN` | ❌ | medium | Stored-function return semantics. |  |
| `OPEN` cursor | ❌ | medium | Cursor open lifecycle. |  |
| `FETCH` cursor | ❌ | medium | Cursor fetch into variables and NOT FOUND handling. |  |
| `CLOSE` cursor | ❌ | medium | Cursor close lifecycle. |  |
| `GET DIAGNOSTICS` | ❌ | medium | Current and stacked diagnostics retrieval. |  |
| `SIGNAL` | ❌ | medium | User-raised SQLSTATE and condition item semantics. |  |
| `RESIGNAL` | ❌ | medium | Handler rethrow and diagnostics mutation. |  |

### 1.4 Account, resource, plugin, maintenance, SHOW, and utility statements

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `ALTER USER` | ❌ | medium | Authentication plugins, passwords, MFA, TLS, resources, lock/expire, comments, attributes, and default roles. |  |
| `CREATE USER` | ❌ | medium | User creation syntax, IF NOT EXISTS, auth factors, TLS, resources, password options, and comments. |  |
| `CREATE ROLE` | ❌ | medium | Role creation syntax and metadata. |  |
| `DROP USER` | ❌ | medium | User deletion syntax and privilege cleanup. |  |
| `DROP ROLE` | ❌ | medium | Role deletion syntax and grant cleanup. |  |
| `GRANT` | ❌ | medium | Privilege and role grants, WITH GRANT OPTION, PROXY, dynamic privileges, and partial revoke semantics. |  |
| `RENAME USER` | ❌ | medium | User rename syntax and privilege metadata. |  |
| `REVOKE` | ❌ | medium | Privilege and role revocation semantics. |  |
| `SET DEFAULT ROLE` | ❌ | medium | Default role assignment. |  |
| `SET PASSWORD` | ❌ | medium | Password assignment semantics. |  |
| `SET ROLE` | ❌ | medium | Active-role selection. |  |
| `CREATE RESOURCE GROUP` | ❌ | low | Thread resource group creation syntax. |  |
| `ALTER RESOURCE GROUP` | ❌ | low | Resource group modification syntax. |  |
| `DROP RESOURCE GROUP` | ❌ | low | Resource group deletion syntax. |  |
| `SET RESOURCE GROUP` | ❌ | low | Thread assignment to resource groups. |  |
| `ANALYZE TABLE` | ❌ | high | Statistics refresh, histogram update/drop, validation, and result-set metadata. |  |
| `CHECK TABLE` | ❌ | high | Table consistency checks and result-set metadata. |  |
| `CHECKSUM TABLE` | ❌ | high | Table checksum syntax and result-set metadata. |  |
| `OPTIMIZE TABLE` | ❌ | high | Table optimization syntax and result-set metadata. |  |
| `REPAIR TABLE` | ❌ | high | Repair syntax and result-set metadata. |  |
| `INSTALL COMPONENT` | ❌ | low | Component installation syntax and diagnostics. |  |
| `UNINSTALL COMPONENT` | ❌ | low | Component uninstallation syntax and diagnostics. |  |
| `INSTALL PLUGIN` | ❌ | low | Plugin installation syntax and diagnostics. |  |
| `UNINSTALL PLUGIN` | ❌ | low | Plugin uninstallation syntax and diagnostics. |  |
| `CLONE` | ❌ | low | Local and remote clone syntax and diagnostics. |  |
| `SET` | 🟡 | top | Variable assignment, user variables, system variables, persisted variables, names, charset, and transaction forms. | `SET NAMES` and `SET CHARACTER SET` are implemented for the initial charset/collation registry; general variable assignment, user variables, persisted variables, and transaction forms are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `SET CHARACTER SET` | 🟡 | top | Connection character-set shorthand semantics. | Implemented for `utf8mb4`, `utf8mb3`, `latin1`, `binary`, and `DEFAULT`, with handle-owned session state and selected-schema/default-schema collation behavior; `SHOW VARIABLES` exposure is deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `SET NAMES` | 🟡 | top | Connection character set and collation semantics. | Implemented for `utf8mb4`, `utf8mb3`, `latin1`, `binary`, optional compatible `COLLATE`, and `DEFAULT`, with MySQL-runtime-verified unknown/incompatible value behavior; numeric error-code exposure and `SHOW VARIABLES` are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `CACHE INDEX` | ❌ | low | MyISAM key cache assignment syntax. |  |
| `FLUSH` | ❌ | medium | FLUSH variants for logs, tables, privileges, status, hosts, optimizer costs, and user resources. |  |
| `KILL` | ❌ | medium | Connection/query kill syntax and diagnostics. |  |
| `LOAD INDEX INTO CACHE` | ❌ | low | MyISAM index preload syntax. |  |
| `RESET` | ❌ | medium | RESET variants for source/replica/persist-style operations exposed by MySQL 8.4. |  |
| `RESET PERSIST` | ❌ | low | Persisted system variable reset syntax. |  |
| `RESTART` | ❌ | low | Server restart syntax and embedded-compatible diagnostics. |  |
| `SHUTDOWN` | ❌ | low | Server shutdown syntax and embedded-compatible diagnostics. |  |
| `DESCRIBE` / `DESC` | ❌ | top | Table, column, and statement description semantics. |  |
| `EXPLAIN` | ❌ | high | Explain SELECT/TABLE/INSERT/UPDATE/DELETE, formats, ANALYZE, and FOR CONNECTION. |  |
| `HELP` | ❌ | low | Server help lookup result-set semantics. |  |
| `USE` | 🟡 | top | Default schema selection in the embedded single-file model. | Implemented as handle-owned session state over the MyLite schema catalog; schema-qualified object execution and privilege checks are deferred. See [schema lifecycle spec](docs/specs/schema-lifecycle/specs.md). |

### 1.5 SHOW statements

| SHOW statement | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `SHOW BINARY LOG STATUS` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW BINARY LOGS` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW BINLOG EVENTS` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CHARACTER SET` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COLLATION` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COLUMNS` / `SHOW FIELDS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COUNT(*) ERRORS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW COUNT(*) WARNINGS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE DATABASE` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE EVENT` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE FUNCTION` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE PROCEDURE` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE TABLE` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE TRIGGER` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE USER` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW CREATE VIEW` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW DATABASES` | 🟡 | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. | Unfiltered `SHOW DATABASES` / `SHOW SCHEMAS` returns MyLite catalog rows with a `Database` column; `LIKE`, `WHERE`, and privilege filtering are deferred. See [schema lifecycle spec](docs/specs/schema-lifecycle/specs.md). |
| `SHOW ENGINE` | ❌ | low | Generic SHOW ENGINE syntax, subcommand dispatch, result-set shape, and engine-specific diagnostics. |  |
| `SHOW ENGINE LOGS` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ENGINE MUTEX` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ENGINE STATUS` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ENGINES` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW ERRORS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW EVENTS` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW FUNCTION CODE` | ❌ | low | Debug-build-only stored-function instruction listing, privileges, result-set shape, and MySQL-compatible diagnostics when unavailable. | Conditional surface; available only for debug-capable builds. |
| `SHOW FUNCTION STATUS` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW GRANTS` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW MASTER STATUS` | ❌ | low | No longer supported in MySQL 8.4; match parser/diagnostic behavior rather than returning binary-log status rows. | Replacement statement is `SHOW BINARY LOG STATUS`. |
| `SHOW OPEN TABLES` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PARSE_TREE` | ❌ | low | JSON parse-tree result for the MySQL 8.4.9 grammar when enabled; syntax error when built without `WITH_SHOW_PARSE_TREE`. | Conditional debug/development surface. |
| `SHOW PLUGINS` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PRIVILEGES` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROCEDURE CODE` | ❌ | low | Debug-build-only stored-procedure instruction listing, privileges, result-set shape, and MySQL-compatible diagnostics when unavailable. | Conditional surface; available only for debug-capable builds. |
| `SHOW PROCEDURE STATUS` | ❌ | medium | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROCESSLIST` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROFILE` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW PROFILES` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW RELAYLOG EVENTS` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW REPLICA STATUS` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW REPLICAS` | ❌ | low | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW STATUS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW TABLE STATUS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW TABLES` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW TRIGGERS` | ❌ | high | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW VARIABLES` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |
| `SHOW WARNINGS` | ❌ | top | Result-set shape, filtering, LIKE/WHERE clauses where supported, privileges, and MySQL 8.4 deprecation/removal behavior. |  |

## 2. Query expressions and SELECT semantics

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| Query expression grammar | 🟡 | top | Parenthesized query expressions, query terms, and query primary rules. | Implemented for the first top-level `UNION` query-expression slice over already-supported `SELECT` operands, including parenthesized SELECT operands with operand-local `ORDER BY`/`LIMIT` and global `ORDER BY`/`LIMIT`/`OFFSET`. CTEs, derived query expressions, `INTERSECT`, `EXCEPT`, standalone `TABLE`, standalone `VALUES`, query expressions in subqueries, and broader parenthesized query expression forms remain deferred. See [UNION query expressions spec](docs/specs/union-query-expressions/specs.md). |
| `WITH` common table expressions | ❌ | high | Non-recursive CTEs, column lists, name shadowing, and scope. |  |
| `WITH RECURSIVE` | ❌ | high | Recursive CTE execution, cycle behavior, limits, and type inference. |  |
| Projection list | 🟡 | top | Expression aliases, wildcard expansion, qualified wildcards, duplicate names, and metadata. | Implemented for direct column-reference projection aliases, supported Task 16 table-backed projection expressions and literals, Task 24 pure scalar function expressions, Task 25 CASE expressions, Task 25 `COUNT`/`SUM`/`AVG`/`MIN`/`MAX` aggregate outputs, Task 29 uncorrelated scalar, `EXISTS`, scalar `IN`/`NOT IN`, scalar quantified-comparison, row scalar-comparison, row `IN`/`NOT IN`, and row quantified alias subquery expressions, duplicate output labels, one-table wildcards, Task 26 base-table inner/comma join `*`, qualified wildcard expansion, and `USING` coalesced columns, and Task 27 outer join wildcard/coalesced output, with public label/schema/table/origin accessors and Task 23 type, flag, length, decimals, charset, and nullability descriptors. Remaining function families, correlated subqueries, broader quantified subquery shapes, set-operation projection merging beyond the current top-level `UNION` slice, and projection behavior for deferred join surfaces remain deferred. See [table-backed SELECT core spec](docs/specs/select-table-core/specs.md), [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md), [result metadata and expression labels spec](docs/specs/result-metadata-expression-labels/specs.md), [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md), [CASE expression spec](docs/specs/case-expression/specs.md), [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md), [inner joins spec](docs/specs/inner-joins/specs.md), [outer joins spec](docs/specs/outer-joins/specs.md), [subqueries spec](docs/specs/subqueries/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md), [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md), and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| Table references | 🟡 | top | Base tables, aliases, schema qualifiers, derived tables, table functions, and parentheses. | Implemented for user base tables with selected-schema or schema-qualified resolution, optional `AS`/bare table aliases, and Task 26/27 multi-table SELECT row sources using inner/cross/comma and `LEFT`/`RIGHT` outer joins. Derived tables, table functions, partitions, index hints, and parenthesized table references remain deferred. See [table-backed SELECT core spec](docs/specs/select-table-core/specs.md), [inner joins spec](docs/specs/inner-joins/specs.md), and [outer joins spec](docs/specs/outer-joins/specs.md). |
| Inner joins | 🟡 | top | JOIN, INNER JOIN, CROSS JOIN, ON, and USING. | Implemented for SELECT over user base tables, including aliases, explicit join operand scoping, optional `ON` predicates over the supported scalar expression subset, optional `USING` column validation/coalescing/name resolution, wildcard expansion, result metadata, and WHERE/ORDER/LIMIT/aggregate interaction. Natural joins, STRAIGHT_JOIN, derived tables, parenthesized join groups, index hints, and optimizer behavior remain deferred. See [inner joins spec](docs/specs/inner-joins/specs.md). |
| Comma joins | 🟡 | top | Comma-separated table references, Cartesian products, and precedence with explicit joins. | Implemented for SELECT over user base tables, including Cartesian products, WHERE filtering, lower comma precedence relative to explicit joins, and `ON` visibility diagnostics for comma-left tables outside an explicit join operand. Parenthesized join groups and optimizer behavior remain deferred. See [inner joins spec](docs/specs/inner-joins/specs.md). |
| Outer joins | 🟡 | top | LEFT/RIGHT OUTER JOIN null-extension and predicate placement. | Implemented for SELECT over user base tables, including `LEFT`/`RIGHT` with optional `OUTER`, required `ON` or `USING`, null extension of the non-preserved side, `ON` before null extension and `WHERE` after it, aliases, explicit join operand scoping, `USING` validation/coalescing/name resolution, wildcard expansion, result metadata nullability, warning-count preservation, and ORDER/LIMIT/no-group aggregate interaction. Natural joins, STRAIGHT_JOIN, derived tables, parenthesized join groups, index hints, optimizer behavior, and grouped joined queries remain deferred. See [outer joins spec](docs/specs/outer-joins/specs.md). |
| Natural joins | ❌ | high | NATURAL INNER/LEFT/RIGHT JOIN column matching and metadata. |  |
| `STRAIGHT_JOIN` | ❌ | medium | Join-order forcing syntax and optimizer interaction. |  |
| Lateral derived tables | ❌ | medium | LATERAL derived table correlation rules. |  |
| `WHERE` | 🟡 | top | Predicate semantics, type conversion, three-valued logic, and short-circuit-sensitive warnings. | Implemented for one user base table in table-backed `SELECT`, over the Task 16 expression subset, Task 24 pure scalar function subset, Task 25 CASE expressions, and Task 29 uncorrelated scalar/`EXISTS`/scalar `IN`/`NOT IN`/scalar quantified-comparison/row scalar-comparison/row `IN`/`NOT IN`/row quantified alias subqueries, with MyLite-owned row scanning, predicate name resolution, three-valued filtering, conversion and division warnings, invalid `ESCAPE` diagnostics, warning lifecycle, and projection metadata preservation. Task 26/27 also apply the supported predicate subset after base-table join production and outer-join null extension, with `ON` predicates scoped to explicit join operands and evaluated before null extension. Single-table `UPDATE` and `DELETE` predicates use the same supported scalar subset without subquery execution. Correlated subqueries, broader quantified subquery shapes, remaining function families, broad collations, no-table/`DUAL` predicates, information-schema filters, optimizer pushdown, index use, and predicate behavior for deferred join surfaces remain deferred. See [WHERE clause spec](docs/specs/where-clause/specs.md), [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md), [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md), [CASE expression spec](docs/specs/case-expression/specs.md), [inner joins spec](docs/specs/inner-joins/specs.md), [outer joins spec](docs/specs/outer-joins/specs.md), [subqueries spec](docs/specs/subqueries/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md), [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md), and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| `GROUP BY` | 🟡 | top | Grouping expression semantics, ordinals, aliases, functional-dependence handling, and ONLY_FULL_GROUP_BY. | Implemented for the first single-table aggregate slice with scalar group expressions, selected aliases, one-based ordinals, ambiguity warnings, conservative `ONLY_FULL_GROUP_BY` checks, result metadata, and MySQL-runtime-covered rows/errors/warnings. Broad functional-dependence inference, joins/subqueries, rollup/grouping sets, collations, and optimizer pushdown remain deferred. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| `WITH ROLLUP` | ❌ | medium | Super-aggregate rows and GROUPING() behavior. |  |
| `HAVING` | 🟡 | top | Post-group predicate semantics and alias resolution. | Implemented for the first single-table aggregate slice, including post-group filtering, aggregate calls, select-alias references, implicit-group behavior, Task 29 uncorrelated scalar/`EXISTS`/scalar `IN`/`NOT IN`/scalar quantified-comparison/row scalar-comparison/row `IN`/`NOT IN`/row quantified alias subqueries, and ordering before `ORDER BY`/`LIMIT`. Outer references, correlated subqueries, broader quantified subquery shapes, broad functional dependence, and advanced collation cases remain deferred. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md), [subqueries spec](docs/specs/subqueries/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md), [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md), and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| Window definitions | ❌ | high | WINDOW clause, named windows, inheritance, partitioning, ordering, frames, and MySQL restrictions. |  |
| `ORDER BY` | 🟡 | top | Expression, alias, ordinal, collation, ASC/DESC, and filesort metadata behavior. | Implemented for one-table `SELECT` after optional `WHERE`, for the Task 25 aggregate grouping slice after `HAVING`, and for Task 26/27 base-table joins after joined row production, null extension, and `WHERE`, with MyLite-owned sorting, multiple keys, `ASC`/`DESC`, `NULL` ordering, unqualified alias-before-column lookup, qualified table-column lookup, duplicate-alias ambiguity diagnostics, one-based ordinals, hidden supported sort expressions including Task 24 pure scalar functions, Task 25 CASE expressions, aggregate calls, and Task 29 uncorrelated scalar/`EXISTS`/scalar `IN`/`NOT IN`/scalar quantified-comparison/row scalar-comparison/row `IN`/`NOT IN`/row quantified alias subqueries, warning preservation before `LIMIT`, and projection metadata preservation. Single-table `UPDATE` and `DELETE` order keys use the same supported scalar subset without subquery execution. Top-level `UNION` query expressions support global `ORDER BY` over result labels, ordinals, and supported expressions. `INTERSECT`, `EXCEPT`, no-table/`DUAL` ordering outside parenthesized `UNION` operands, full collation fidelity, correlated subqueries, broader quantified subquery shapes, remaining function families, broader expression metadata, and ordering behavior for deferred join surfaces remain deferred. See [ORDER BY, LIMIT, and OFFSET spec](docs/specs/order-limit-offset/specs.md), [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md), [CASE expression spec](docs/specs/case-expression/specs.md), [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md), [inner joins spec](docs/specs/inner-joins/specs.md), [outer joins spec](docs/specs/outer-joins/specs.md), [subqueries spec](docs/specs/subqueries/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md), [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md), and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| `LIMIT` / `OFFSET` | 🟡 | top | Row limiting, prepared markers, integer conversion, and error cases. | Implemented for one-table `SELECT` and top-level `UNION` query expressions as literal `LIMIT row_count`, `LIMIT offset,row_count`, and `LIMIT row_count OFFSET offset`, with normalized offset/count AST, unsigned 64-bit literal validation, syntax errors for negative/decimal/string/`NULL`/expression/overflow bounds and direct `LIMIT ?`, `LIMIT 0` metadata preservation, and read-only `SELECT` side effects. Prepared-statement parameter markers, plain no-table/`DUAL` `SELECT` limiting outside parenthesized `UNION` operands, and use outside the current one-table `SELECT` and top-level `UNION` subsets remain deferred. See [ORDER BY, LIMIT, and OFFSET spec](docs/specs/order-limit-offset/specs.md) and [UNION query expressions spec](docs/specs/union-query-expressions/specs.md). |
| `DISTINCT` / `DISTINCTROW` | 🟡 | top | Duplicate elimination semantics and metadata. | Implemented for no-table scalar SELECT and current table-backed SELECT surfaces, including `ALL`, repeated same-mode modifiers, mixed-mode error 1221, projected-row duplicate comparison, `NULL` duplicate collapsing, supported collation-sensitive text comparison, metadata preservation, joins, aggregates, `ORDER BY`/`LIMIT`, and DISTINCT/order error 3065 for hidden non-selected columns. Top-level `UNION` supports default and explicit `DISTINCT` duplicate modes over the current union slice. Aggregate-local `DISTINCT`, non-UNION set-operation duplicate modes, derived tables, subqueries, optimizer pushdown, and deferred row sources remain deferred. See [SELECT DISTINCT spec](docs/specs/select-distinct/specs.md) and [UNION query expressions spec](docs/specs/union-query-expressions/specs.md). |
| `UNION` | 🟡 | high | ALL/DISTINCT semantics, column names/types, ordering, limits, and parentheses. | Implemented for top-level `SELECT ... UNION [ALL\|DISTINCT] SELECT ...` over already-supported SELECT surfaces, including default `DISTINCT`, explicit `ALL`/`DISTINCT`, left-associated mixed-mode chains, duplicate elimination with `NULL` collapsing and descriptor-sensitive text comparison, column-count error 1222, first-operand labels, empty union origin metadata, first-slice descriptor aggregation, global `ORDER BY`/`LIMIT`/`OFFSET`, table-qualified global-order error 1250, operand warning preservation, and parenthesized operands with local `ORDER BY`/`LIMIT`. `INTERSECT`, `EXCEPT`, standalone `TABLE`/`VALUES`, CTEs, query expressions in subqueries, and optimizer pushdown remain deferred. See [UNION query expressions spec](docs/specs/union-query-expressions/specs.md). |
| `INTERSECT` | ❌ | medium | MySQL 8.4 set operator semantics. |  |
| `EXCEPT` | ❌ | medium | MySQL 8.4 set operator semantics. |  |
| Scalar subqueries | 🟡 | top | Single-value cardinality, NULL behavior, correlation, and errors. | Implemented for uncorrelated scalar subqueries in no-table scalar SELECT and current table-backed SELECT projection, `WHERE`, `ON`, `HAVING`, and `ORDER BY` expression contexts, including empty-result `NULL`, one-row value return, error 1241 for multi-column scalar operands, error 1242 for multi-row scalar operands, warnings, and first-slice metadata. Multi-column row scalar subqueries are tracked under row subqueries. Correlation, DML contexts, optimizer behavior, and broader metadata fidelity remain deferred. See [subqueries spec](docs/specs/subqueries/specs.md). |
| Row subqueries | 🟡 | high | Row constructors and multi-column comparison semantics. | Implemented for uncorrelated multi-element `(a,b)` and `ROW(a,b)` scalar row subquery comparisons with `=`, `<>`, `!=`, `<`, `<=`, `>`, `>=`, and `<=>`, row `IN` / `NOT IN` subqueries, and the MySQL-accepted row quantified aliases row `= ANY`, row `= SOME`, row `<> ALL`, and row `!= ALL` in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY` contexts. Coverage includes tuple-width error 1241, row scalar cardinality error 1242, row membership inner `LIMIT` error 1235, MySQL tuple `NULL` truth semantics, conversion warnings, nullable boolean metadata for nullable row predicates, `NOT_NULL` metadata for null-safe row scalar comparisons, and explicit deferred diagnostics for correlation and DML execution. One-element `ROW(expr)`, general non-alias row quantified comparisons, row `<=> ANY`, correlation, DML execution, derived row sources, CTEs, set operations, and optimizer behavior remain deferred. See [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md) and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| `EXISTS` subqueries | 🟡 | top | Existence semantics and correlation behavior. | Implemented for uncorrelated `EXISTS` and `NOT EXISTS` in no-table scalar SELECT and current table-backed SELECT projection, `WHERE`, `ON`, `HAVING`, and `ORDER BY` expression contexts, including select-list-independent existence checks for the first executable slice and boolean metadata. Correlation, DML contexts, optimizer transformations, and broader subquery row-source coverage remain deferred. See [subqueries spec](docs/specs/subqueries/specs.md). |
| `IN` subqueries | 🟡 | top | NULL-aware membership semantics and type conversion. | Implemented for Task 29's first scalar and row membership slices: uncorrelated scalar `IN` / `NOT IN` subqueries and multi-element row `IN` / `NOT IN` subqueries in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY` contexts. Coverage includes empty-result and `NULL` truth semantics, conversion warnings, error 1241, inner `LIMIT` error 1235, ignored inner `ORDER BY` execution, and boolean metadata. Correlation, DML contexts, derived tables, CTEs, set operations, semijoin/antijoin optimization, and broader metadata fidelity remain deferred. See [subqueries spec](docs/specs/subqueries/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), and [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md). |
| `ANY` / `SOME` / `ALL` subqueries | 🟡 | high | Quantified comparison semantics. | Implemented for uncorrelated scalar left operands with one-column subqueries and for the MySQL-accepted row aliases row `= ANY`, row `= SOME`, row `<> ALL`, and row `!= ALL` in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY` contexts. General row quantified comparisons, row `<=> ANY`, correlation, DML contexts, derived tables, CTEs, set operations, and optimizer behavior remain deferred. See [subqueries spec](docs/specs/subqueries/specs.md), [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md), and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| Derived table materialization/merge | ❌ | high | Optimizer-visible semantics and metadata results. |  |
| Index hints | ❌ | high | USE/FORCE/IGNORE INDEX with FOR JOIN/ORDER BY/GROUP BY scopes. |  |
| Optimizer hints | ❌ | medium | Comment-hint grammar and ignored/accepted hint diagnostics. |  |
| `PARTITION` selection | ❌ | low | Explicit partition selection syntax and errors. |  |
| Locking clauses | ❌ | high | FOR UPDATE, FOR SHARE, OF table list, NOWAIT, and SKIP LOCKED. |  |
| SELECT modifiers | ❌ | top | ALL, HIGH_PRIORITY, SQL_SMALL_RESULT, SQL_BIG_RESULT, SQL_BUFFER_RESULT, SQL_CALC_FOUND_ROWS, and STRAIGHT_JOIN. | `ALL` is implemented for duplicate-mode parsing as part of Task 28. The remaining optimizer and execution-strategy modifiers are deferred. See [SELECT DISTINCT spec](docs/specs/select-distinct/specs.md). |
| Expression metadata | 🟡 | top | Column type, length, decimals, flags, charset, collation, nullability, and origin metadata. | Implemented for current scalar `SELECT` output and table-backed `SELECT` projections: base columns, aliases, duplicate labels, wildcards, invisible-column wildcard omission, supported Task 16 literals and operators, Task 24 pure scalar function descriptors, Task 25 CASE result descriptor aggregation, Task 25 aggregate descriptors for `COUNT`/`SUM`/`AVG`/`MIN`/`MAX`, Task 26 base-table join wildcard and column-origin metadata, Task 27 outer join null-extended side nullability and preserved-side `USING` origin metadata, Task 29 first-slice scalar, `EXISTS`, scalar `IN`/`NOT IN`, scalar quantified-comparison, row scalar-comparison, and row `IN`/`NOT IN` subquery descriptors, and hidden `ORDER BY` keys that do not change visible metadata. The public C API exposes MySQL-style field type, flags, declared length, max length placeholder, decimals, charset id, nullability, schema, table, origin table, and origin column metadata. Derived tables, broader quantified subquery descriptors, remaining function families, prepared-statement metadata, protocol packets, materialized `max_length`, exhaustive collation coercion, and metadata behavior for deferred join surfaces remain deferred. See [table-backed SELECT core spec](docs/specs/select-table-core/specs.md), [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md), [result metadata and expression labels spec](docs/specs/result-metadata-expression-labels/specs.md), [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md), [CASE expression spec](docs/specs/case-expression/specs.md), [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md), [inner joins spec](docs/specs/inner-joins/specs.md), [outer joins spec](docs/specs/outer-joins/specs.md), [subqueries spec](docs/specs/subqueries/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md), and [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md). |
| Scalar built-in functions | 🟡 | top | Common string, numeric, temporal, conditional, comparison, and information scalar functions with MySQL-compatible type conversion, warnings, errors, metadata, and statement/session state. | Task 24 now implements the first pure deterministic scalar subset: `CONCAT`, `LENGTH`/`OCTET_LENGTH`, `CHAR_LENGTH`/`CHARACTER_LENGTH`, `LOWER`/`LCASE`, `UPPER`/`UCASE`, `LEFT`, `RIGHT`, `REPLACE`, `ABS`, `SIGN`, `FLOOR`, `CEIL`/`CEILING`, `MOD`, `PI`, `IF`, `IFNULL`, `NULLIF`, `COALESCE`, and `ISNULL`. They work in no-table `SELECT`, one-table `SELECT` projection/`WHERE`/`ORDER BY`, and supported single-table `UPDATE`/`DELETE` expression paths, with runtime tests for result rows, NULLs, warnings, metadata, table predicates/order, and DML predicates/assignments. Exact MySQL error codes for unsupported functions/arity, broad type/collation coercion, `INSERT` expression paths, temporal/info functions, remaining aggregate and window functions, JSON, regex, spatial, full-text, encryption, loadable, and server-administration functions remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |

## 3. DDL detail surface

### 3.1 CREATE TABLE, index, and constraint features

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| Column definition grammar | 🟡 | top | Type, nullability, defaults, visibility, comments, storage, format, references, and constraints. | Supported `CREATE TABLE` column definitions now execute for integer, boolean, string, binary, exact numeric, approximate numeric, and temporal column types plus common column attributes, writing SQLite storage and MyLite column metadata. Generated columns, references, checks, physical default evaluation, physical constraint enforcement, full semantic validation, and warning records are deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md), [string and binary column types spec](docs/specs/string-binary-column-types/specs.md), [numeric column types spec](docs/specs/numeric-column-types/specs.md), [temporal column types spec](docs/specs/temporal-column-types/specs.md), [column attributes spec](docs/specs/column-attributes/specs.md), [primary keys and AUTO_INCREMENT spec](docs/specs/primary-keys-auto-increment/specs.md), [CREATE TABLE unique and secondary indexes spec](docs/specs/create-table-indexes/specs.md), and [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Silent column specification changes | ❌ | high | MySQL automatic rewrites of column specifications and SHOW CREATE output. |  |
| Default expressions | 🟡 | top | Literal and expression defaults, CURRENT_TIMESTAMP variants, and error cases. | `CREATE TABLE` records supported literal, parenthesized-expression, and targeted `CURRENT_TIMESTAMP[(fsp)]` default text in column metadata, including `DEFAULT_GENERATED` for generated defaults. `INSERT ... VALUES` and `INSERT ... SET` evaluate deterministic null, literal, unary numeric, string, and current-timestamp defaults. Arbitrary generated default expressions and function calls remain deferred and fail deterministically when needed during insert. Type-specific conversion diagnostics, subqueries, variables, and warning records are deferred. See [column attributes spec](docs/specs/column-attributes/specs.md), [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md), [INSERT ... VALUES spec](docs/specs/insert-values/specs.md), and [INSERT ... SET spec](docs/specs/insert-set/specs.md). |
| Generated columns | ❌ | high | Virtual/stored generated columns, dependencies, indexes, and metadata. |  |
| Invisible columns | 🟡 | medium | Implicit column lists, SELECT * behavior, and metadata flags. | `CREATE TABLE` records `VISIBLE` and `INVISIBLE` column attributes, exposes invisible columns through `INFORMATION_SCHEMA.COLUMNS.EXTRA`, omits invisible columns from Task 15 wildcard SELECT expansion, and allows explicit invisible-column selection. All-invisible table validation, implicit column-list behavior, and `SHOW CREATE TABLE` formatting remain deferred. See [column attributes spec](docs/specs/column-attributes/specs.md), [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md), and [table-backed SELECT core spec](docs/specs/select-table-core/specs.md). |
| Column comments | 🟡 | high | Column `COMMENT` attributes, metadata, length checks, and SHOW output. | `CREATE TABLE` records supported column `COMMENT 'string'` attributes into column metadata and exposes them through `INFORMATION_SCHEMA.COLUMNS.COLUMN_COMMENT`. Length validation, escaping normalization, `SHOW FULL COLUMNS`, and warning records are deferred. See [column attributes spec](docs/specs/column-attributes/specs.md) and [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Column storage/format attributes | ⚪ | low | NDB-oriented `COLUMN_FORMAT` and `STORAGE` column attributes and diagnostics. | Parser accepts `COLUMN_FORMAT DEFAULT/FIXED/DYNAMIC` and `STORAGE DEFAULT/DISK/MEMORY` in parse-only column definitions and rejects malformed syntax such as `=` forms or unknown values; engine-specific effects, warnings, and `SHOW CREATE TABLE` formatting are deferred. See [column attributes spec](docs/specs/column-attributes/specs.md). |
| Generated invisible primary keys | ❌ | low | MySQL-generated invisible primary key behavior, metadata, and dump/replication interactions. |  |
| AUTO_INCREMENT columns | 🟡 | top | Allocation, persistence, lock modes, explicit values, and overflow behavior. | Inline `AUTO_INCREMENT` and table-level `AUTO_INCREMENT` execute into metadata for supported `CREATE TABLE` statements. `INSERT ... VALUES` and `INSERT ... SET` allocate omitted, `NULL`, `0`, and `DEFAULT` auto values, advance after accepted explicit nonzero values, persist consumed generated values after failed statement rollback, persist the next catalog value, and expose the first generated id through `mylite_last_insert_id()`; `INSERT ... VALUES` also reserves simple multi-row generated intervals. SQL-mode variants such as `NO_AUTO_VALUE_ON_ZERO`, overflow fidelity, lock modes, full type/key/default semantic diagnostics, and warning records remain deferred. See [primary keys and AUTO_INCREMENT spec](docs/specs/primary-keys-auto-increment/specs.md), [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md), [INSERT ... VALUES spec](docs/specs/insert-values/specs.md), and [INSERT ... SET spec](docs/specs/insert-set/specs.md). |
| Primary keys | 🟡 | top | Definition forms, implicit NOT NULL behavior, metadata, and errors. | Inline `PRIMARY KEY`, inline `KEY`, and table-level `PRIMARY KEY` constraints now execute into column and statistics metadata, including duplicate-primary diagnostics and implicit metadata `NOT NULL`. `INSERT ... VALUES` enforces deterministic primary/unique duplicate checks for inserted values. Full physical index storage, type/prefix suitability checks, `SHOW CREATE TABLE`, and storage-engine warnings are deferred. See [primary keys and AUTO_INCREMENT spec](docs/specs/primary-keys-auto-increment/specs.md), [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md), and [INSERT ... VALUES spec](docs/specs/insert-values/specs.md). |
| Unique indexes | 🟡 | top | NULL handling, prefix lengths, functional key parts, and conflict semantics. | Inline `UNIQUE`/`UNIQUE KEY` and supported table-level unique indexes now execute into statistics metadata with deterministic generated names and duplicate explicit-name checks. Physical uniqueness enforcement, functional key parts, full prefix suitability checks, `SHOW CREATE TABLE`, and warning records are deferred. See [CREATE TABLE unique and secondary indexes spec](docs/specs/create-table-indexes/specs.md) and [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Nonunique indexes | 🟡 | top | BTREE/HASH clauses, visibility, comments, and parser options. | Supported table-level `KEY` and `INDEX` forms now execute into statistics metadata with index names, key parts, prefix lengths, order, visibility, type, and comments. Physical SQLite index creation, optimizer behavior, storage-engine diagnostics, and warning records are deferred. See [CREATE TABLE unique and secondary indexes spec](docs/specs/create-table-indexes/specs.md) and [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Descending indexes | 🟡 | high | DESC key-part syntax and ordering semantics. | `ASC` and `DESC` key-part markers are exposed through `INFORMATION_SCHEMA.STATISTICS.COLLATION` for supported `CREATE TABLE` indexes; physical order and optimizer behavior are deferred. See [CREATE TABLE unique and secondary indexes spec](docs/specs/create-table-indexes/specs.md) and [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Prefix indexes | 🟡 | top | Prefix length parsing, byte/character semantics, and limits. | Integer prefix lengths are exposed through `INFORMATION_SCHEMA.STATISTICS.SUB_PART` for supported `CREATE TABLE` indexes; type suitability, byte/character semantics, storage limits, and warnings are deferred. See [CREATE TABLE unique and secondary indexes spec](docs/specs/create-table-indexes/specs.md) and [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Functional key parts | ❌ | high | Expression key parts and metadata. |  |
| Multi-valued indexes | ❌ | medium | JSON array multi-valued index syntax, casts, optimizer behavior, and metadata. |  |
| FULLTEXT indexes | ❌ | high | Index DDL, parser options, MATCH metadata, and embedded search behavior. |  |
| SPATIAL indexes | ❌ | medium | Geometry column requirements and spatial metadata. |  |
| Foreign keys | ❌ | high | Definition, matching, cascades, restrict/no action, set null/default, checks, and metadata. |  |
| CHECK constraints | ❌ | high | Expression validation, enforcement, names, and metadata. |  |
| Constraint naming | ❌ | top | Generated names, duplicate handling, schema scope, and SHOW CREATE output. |  |
| Table options: engine | 🟡 | top | ENGINE, SECONDARY_ENGINE, engine attributes, and unsupported-engine diagnostics. | `CREATE TABLE` accepts and executes `ENGINE [=] InnoDB`, records `ENGINE='InnoDB'`, and returns a deterministic unsupported-engine execution diagnostic for other parsed engine names. Other engine-related table options remain unsupported. See [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Table options: charset/collation | 🟡 | top | DEFAULT CHARACTER SET, COLLATE, and conversion-sensitive metadata. | `CREATE TABLE` accepts and executes `[DEFAULT] CHARACTER SET`/`CHARSET` and `[DEFAULT] COLLATE` for the initial charset/collation registry, validates unknown and mismatched values, and records effective table/column metadata. Full charset catalog coverage and conversion-sensitive storage semantics are deferred. See [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| Table options: storage | ❌ | high | ROW_FORMAT, COMPRESSION, ENCRYPTION, TABLESPACE, KEY_BLOCK_SIZE, and directories. |  |
| Table options: statistics | ❌ | high | STATS_AUTO_RECALC, STATS_PERSISTENT, STATS_SAMPLE_PAGES, AVG_ROW_LENGTH, MAX_ROWS, MIN_ROWS. |  |
| Table options: misc | 🟡 | high | AUTO_INCREMENT, COMMENT, CHECKSUM, DELAY_KEY_WRITE, INSERT_METHOD, PACK_KEYS, PASSWORD, UNION. | `CREATE TABLE` accepts and executes table `AUTO_INCREMENT [=] integer` and `COMMENT [=] 'string'` into metadata. Allocation behavior, warnings, and other miscellaneous table options remain unsupported. See [CREATE TABLE base execution spec](docs/specs/create-table-base-execution/specs.md). |
| NDB comment options | ❌ | low | NDB-specific table and column comment options recognized by MySQL grammar. |  |
| Temporary table metadata | ❌ | top | Session isolation, name shadowing, and cleanup. |  |
| CREATE INDEX options | ❌ | top | ALGORITHM, LOCK, visibility, comments, and index type clauses. | Started/speced only for standalone `CREATE INDEX`; no parser or runtime support yet. The intended first slice accepts `USING`/`TYPE`, `KEY_BLOCK_SIZE`, `COMMENT`, visibility, engine attributes, and embedded no-op `ALGORITHM`/`LOCK` modifiers while deferring full storage-engine warning fidelity. See [standalone CREATE INDEX and DROP INDEX spec](docs/specs/create-drop-index/specs.md). |
| CREATE VIEW options | ❌ | high | ALGORITHM, DEFINER, SQL SECURITY, column list, and WITH CHECK OPTION. |  |

### 3.2 ALTER TABLE actions

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `ADD COLUMN` | ❌ | top | Positioning, FIRST/AFTER, multiple columns, generated/invisible/default handling. |  |
| `DROP COLUMN` | ❌ | top | Dependency checks, generated columns, indexes, constraints, and errors. |  |
| `RENAME COLUMN` | ❌ | top | Metadata rewrite and dependency updates. |  |
| `CHANGE COLUMN` | ❌ | top | Rename plus type/attribute change semantics. |  |
| `MODIFY COLUMN` | ❌ | top | Type/attribute change without rename. |  |
| `ALTER COLUMN SET DEFAULT` | ❌ | top | Default mutation semantics. |  |
| `ALTER COLUMN DROP DEFAULT` | ❌ | top | Default removal semantics. |  |
| `ALTER COLUMN SET VISIBLE` / `SET INVISIBLE` | ❌ | medium | Column visibility changes and restrictions. |  |
| `ADD PRIMARY KEY` | ❌ | top | Primary key addition and data validation. |  |
| `DROP PRIMARY KEY` | ❌ | top | Primary key removal and auto-increment restrictions. |  |
| `ADD UNIQUE` | ❌ | top | Unique index addition and duplicate validation. |  |
| `ADD INDEX` / `ADD KEY` | ❌ | top | Secondary index addition and metadata. |  |
| `ADD FULLTEXT` | ❌ | high | Full-text index addition. |  |
| `ADD SPATIAL` | ❌ | medium | Spatial index addition. |  |
| `DROP INDEX` / `DROP KEY` | ❌ | top | Index removal and constraint dependencies. | Standalone `DROP INDEX` is started/speced in [standalone CREATE INDEX and DROP INDEX spec](docs/specs/create-drop-index/specs.md); `DROP KEY` remains an `ALTER TABLE` key-action surface for the later ALTER TABLE task. |
| `RENAME INDEX` / `RENAME KEY` | ❌ | high | Index rename semantics. |  |
| `ALTER INDEX VISIBLE` / `INVISIBLE` | ❌ | medium | Index visibility metadata and optimizer behavior. |  |
| `ADD CONSTRAINT CHECK` | ❌ | high | Check constraint addition and validation. |  |
| `DROP CHECK` | ❌ | high | Check constraint removal. |  |
| `ALTER CHECK ENFORCED` / `NOT ENFORCED` | ❌ | high | Check enforcement toggling. |  |
| `ADD CONSTRAINT FOREIGN KEY` | ❌ | high | Foreign key addition, validation, indexes, and actions. |  |
| `DROP FOREIGN KEY` | ❌ | high | Foreign key removal and metadata cleanup. |  |
| `DISABLE KEYS` / `ENABLE KEYS` | ❌ | low | MyISAM-style key maintenance syntax and diagnostics. |  |
| `RENAME TO` | ❌ | top | Table rename via ALTER TABLE. |  |
| `ORDER BY` | ❌ | medium | Physical row ordering syntax and embedded-compatible behavior. |  |
| `CONVERT TO CHARACTER SET` | ❌ | high | Column/table charset and collation conversion semantics. |  |
| `DEFAULT CHARACTER SET` / `COLLATE` | ❌ | high | Table default charset/collation changes. |  |
| `FORCE` | ❌ | medium | Forced table rebuild semantics. |  |
| `DISCARD TABLESPACE` | ❌ | low | Tablespace discard syntax. |  |
| `IMPORT TABLESPACE` | ❌ | low | Tablespace import syntax. |  |
| `ALGORITHM` | ❌ | high | DEFAULT, INSTANT, INPLACE, COPY handling and diagnostics. |  |
| `LOCK` | ❌ | high | DEFAULT, NONE, SHARED, EXCLUSIVE handling and diagnostics. |  |
| Partition maintenance | ❌ | low | ADD, DROP, DISCARD, IMPORT, TRUNCATE, COALESCE, REORGANIZE, EXCHANGE, ANALYZE, CHECK, OPTIMIZE, REBUILD, REPAIR, REMOVE PARTITIONING. |  |

### 3.3 Partitioning surface

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `PARTITION BY RANGE` | ❌ | medium | Range partition syntax, VALUES LESS THAN, MAXVALUE, and pruning-visible metadata. |  |
| `PARTITION BY RANGE COLUMNS` | ❌ | medium | Range columns syntax and comparison semantics. |  |
| `PARTITION BY LIST` | ❌ | medium | List partition syntax and value matching. |  |
| `PARTITION BY LIST COLUMNS` | ❌ | medium | List columns syntax and tuple matching. |  |
| `PARTITION BY HASH` | ❌ | medium | Hash partition syntax and function semantics. |  |
| `PARTITION BY LINEAR HASH` | ❌ | medium | Linear hash partition syntax. |  |
| `PARTITION BY KEY` | ❌ | medium | Key partition syntax and default key selection. |  |
| `PARTITION BY LINEAR KEY` | ❌ | medium | Linear key partition syntax. |  |
| Subpartitioning | ❌ | low | HASH/KEY subpartition syntax and metadata. |  |
| Partition options | ❌ | low | ENGINE, COMMENT, DATA/INDEX DIRECTORY, MAX_ROWS, MIN_ROWS, TABLESPACE, and nodegroup syntax. |  |

## 4. Type system, literals, and conversion

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `TINYINT` | 🟡 | top | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. | Internal descriptor normalizes signed and unsigned ranges, precision, scale, `DATA_TYPE`, and MySQL 8.4.9 `COLUMN_TYPE`; parse-only column declarations are accepted. `ZEROFILL`, table DDL execution, catalog writes, and value enforcement are deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| `SMALLINT` | 🟡 | high | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. | Internal descriptor normalizes signed and unsigned ranges, precision, scale, `DATA_TYPE`, and MySQL 8.4.9 `COLUMN_TYPE`; parse-only column declarations are accepted. `ZEROFILL`, table DDL execution, catalog writes, and value enforcement are deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| `MEDIUMINT` | 🟡 | top | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. | Internal descriptor normalizes signed and unsigned ranges, precision, scale, `DATA_TYPE`, and MySQL 8.4.9 `COLUMN_TYPE`; parse-only column declarations are accepted. `ZEROFILL`, table DDL execution, catalog writes, and value enforcement are deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| `INT` / `INTEGER` | 🟡 | top | Signed/unsigned integer range, display width compatibility, zerofill, and metadata. | Internal descriptor normalizes `INTEGER` to `int`, signed and unsigned ranges, precision, scale, `DATA_TYPE`, and MySQL 8.4.9 `COLUMN_TYPE`; parse-only column declarations are accepted. `ZEROFILL`, table DDL execution, catalog writes, and value enforcement are deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| `BIGINT` | 🟡 | top | Signed/unsigned integer range, overflow, unsigned arithmetic, and metadata. | Internal descriptor records signed and unsigned ranges as text, including unsigned max `18446744073709551615`, and normalizes precision, scale, `DATA_TYPE`, and `COLUMN_TYPE`; parse-only column declarations are accepted. Arithmetic, overflow enforcement, table DDL execution, and catalog writes are deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| Integer type aliases | 🟡 | medium | MySQL creation-time rewrites for `INT1`, `INT2`, `INT3`, `INT4`, `INT8`, and `MIDDLEINT`, including DESCRIBE and SHOW CREATE metadata. | Internal descriptor and parser normalize `INT1`, `INT2`, `INT3`, `INT4`, `INT8`, and `MIDDLEINT` to MySQL 8.4.9 canonical integer families. `INT0`, `INT5`, `INT6`, `INT7`, and `INT9` remain rejected. DESCRIBE and SHOW CREATE output wait for executable table DDL. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| Integer signedness attributes | 🟡 | top | `SIGNED` and `UNSIGNED` attribute syntax, range selection, metadata, and value enforcement. | Parser accepts repeated `SIGNED`/`UNSIGNED` on integer types and the descriptor treats any `UNSIGNED` as the effective unsigned type, matching MySQL 8.4.9 metadata. Insert/update range enforcement is deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| Integer display width compatibility | 🟡 | medium | Deprecated integer display width syntax, range checking, metadata rewrites, and SHOW CREATE behavior. | Parser accepts integer display-width syntax and rejects syntactically malformed negative widths; descriptor accepts widths `0..255`, rejects widths above `255`, and preserves MySQL 8.4.9 `tinyint(1)` metadata behavior. `ZEROFILL` and SHOW CREATE output are deferred. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| `DECIMAL` / `NUMERIC` | 🟡 | top | Exact precision math, storage limits, rounding, overflow, and metadata. | Internal descriptor normalizes default and declared precision/scale, aliases, `SIGNED`/`UNSIGNED`, `ZEROFILL`, `DATA_TYPE`, and MySQL 8.4.9 `COLUMN_TYPE`; parse-only column declarations are accepted. Exact storage, rounding, warnings, table DDL execution, and catalog writes are deferred. See [numeric column types spec](docs/specs/numeric-column-types/specs.md). |
| `FIXED` | 🟡 | medium | Alias behavior for `DECIMAL`, including creation-time metadata rewrites. | Internal descriptor and parser normalize `FIXED` to decimal metadata for parse-only declarations. Other-vendor compatibility type. See [numeric column types spec](docs/specs/numeric-column-types/specs.md). |
| `FLOAT` | 🟡 | high | Approximate numeric parsing, rounding, special cases, and metadata. | Internal descriptor supports default `FLOAT`, binary precision selector `FLOAT(p)`, display/scale `FLOAT(M,D)`, `SIGNED`/`UNSIGNED`, `ZEROFILL`, and MySQL 8.4.9 metadata; parse-only column declarations are accepted. Approximate storage, rounding, warnings, table DDL execution, and catalog writes are deferred. See [numeric column types spec](docs/specs/numeric-column-types/specs.md). |
| `DOUBLE` / `REAL` | 🟡 | high | Approximate numeric parsing, rounding, REAL_AS_FLOAT mode, and metadata. | Internal descriptor supports `DOUBLE`, `DOUBLE PRECISION`, default-mode `REAL`, display/scale `DOUBLE(M,D)` and `REAL(M,D)`, numeric attributes, and MySQL 8.4.9 metadata; parse-only column declarations are accepted. `REAL_AS_FLOAT`, value behavior, warnings, table DDL execution, and catalog writes are deferred. See [numeric column types spec](docs/specs/numeric-column-types/specs.md). |
| `FLOAT4` / `FLOAT8` | 🟡 | medium | Alias behavior for `FLOAT` and `DOUBLE`, including creation-time metadata rewrites. | Internal descriptor and parser normalize `FLOAT4` to `float` and `FLOAT8` to `double` metadata for parse-only declarations. Other-vendor compatibility types. See [numeric column types spec](docs/specs/numeric-column-types/specs.md). |
| `BIT` | ❌ | high | Bit-value storage, literals, display, and numeric/string conversion. |  |
| `BOOL` / `BOOLEAN` | 🟡 | top | Alias behavior for TINYINT(1) and expression truth rules. | Parser and descriptor implement column-type alias normalization to signed `tinyint(1)` metadata and reject width/signedness clauses on the aliases. Expression truth rules are deferred to expression semantics work. See [integer and boolean column types spec](docs/specs/integer-boolean-column-types/specs.md). |
| `SERIAL` | ❌ | medium | Alias expansion to BIGINT UNSIGNED NOT NULL AUTO_INCREMENT UNIQUE. |  |
| `DATE` | 🟡 | high | Date range, zero dates, invalid dates, literals, casts, and formatting. | Internal descriptor normalizes parse-only `DATE` declarations to MySQL 8.4.9 `DATA_TYPE`, `COLUMN_TYPE`, storage, range, and null `DATETIME_PRECISION` metadata. Table DDL execution, catalog writes, value storage, zero-date behavior, SQL modes, literals, casts, and formatting are deferred. See [temporal column types spec](docs/specs/temporal-column-types/specs.md). |
| `TIME` | 🟡 | high | Time range, fractional seconds, negative values, casts, and formatting. | Parser and descriptor accept `TIME` and `TIME(fsp)` for `fsp` 0..6, normalize MySQL-compatible `COLUMN_TYPE` and `DATETIME_PRECISION`, and reject overflow precision tokens without wrapping. Table DDL execution, storage, rounding/truncation, warnings, casts, and formatting are deferred. See [temporal column types spec](docs/specs/temporal-column-types/specs.md). |
| `DATETIME` | 🟡 | top | Datetime range, fractional seconds, defaults, casts, and formatting. | Parser and descriptor accept `DATETIME` and `DATETIME(fsp)` for `fsp` 0..6 and normalize parse-only metadata. Defaults, `ON UPDATE`, zero values, SQL modes, table DDL execution, catalog writes, storage, casts, and formatting are deferred. See [temporal column types spec](docs/specs/temporal-column-types/specs.md). |
| `TIMESTAMP` | 🟡 | high | UTC conversion, range, fractional seconds, defaults, and ON UPDATE. | Parser and descriptor accept `TIMESTAMP` and `TIMESTAMP(fsp)` for `fsp` 0..6 and normalize parse-only metadata. Default/nullability behavior, `ON UPDATE`, `explicit_defaults_for_timestamp`, time zone conversion, zero values, storage, table DDL execution, and catalog writes are deferred. See [temporal column types spec](docs/specs/temporal-column-types/specs.md). |
| `YEAR` | 🟡 | medium | Year storage, two-digit handling, casts, and display. | Parser and descriptor accept `YEAR` and deprecated `YEAR(4)`, normalize both to `year` metadata, and reject other widths including overflow integer tokens without wrapping. Deprecation warning 1287, input conversion, table DDL execution, catalog writes, storage, casts, and display semantics are deferred. See [temporal column types spec](docs/specs/temporal-column-types/specs.md). |
| `CHAR` | 🟡 | high | Fixed-length string semantics, padding, charsets, collations, and metadata. | Internal descriptor normalizes length, charset/collation, binary-character-set rewrites, `BINARY` and `BYTE` attributes, and MySQL 8.4.9 `DATA_TYPE`/`COLUMN_TYPE`; parse-only column declarations are accepted. Value storage, padding, warnings, catalog writes, and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `VARCHAR` | 🟡 | top | Variable string semantics, length limits, charsets, collations, and metadata. | Internal descriptor validates required length, charset-sensitive limits, charset/collation metadata, binary-character-set rewrites, and `BINARY`/`BYTE` attributes; parse-only column declarations are accepted. Row-size diagnostics, value storage, warnings, catalog writes, and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `CHARACTER` / `CHARACTER VARYING` | 🟡 | medium | Alias behavior for `CHAR` and `VARCHAR`, including creation-time metadata rewrites. | Parser and descriptor normalize aliases to MySQL-compatible `char` and `varchar` metadata for parse-only declarations. SHOW CREATE output waits for executable table DDL. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `NCHAR` / `NATIONAL CHAR` | 🟡 | medium | Standard national-character aliases using MySQL's predefined `utf8mb3` character set. | Parser and descriptor normalize national character declarations to `char` with `utf8mb3` metadata. Deprecation warnings and SHOW CREATE output are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `NVARCHAR` / `NATIONAL VARCHAR` | 🟡 | medium | Standard national-character aliases using MySQL's predefined `utf8mb3` character set. | Parser and descriptor normalize national varchar declarations to `varchar` with `utf8mb3` metadata. MySQL warning 3720 and SHOW CREATE output are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `BINARY` | 🟡 | high | Fixed-length binary semantics and padding. | Internal descriptor normalizes length, binary metadata, and MySQL 8.4.9 `COLUMN_TYPE`; parse-only declarations are accepted. Padding/value storage and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `VARBINARY` | 🟡 | high | Variable binary semantics and length limits. | Internal descriptor validates required length and binary metadata; parse-only declarations are accepted. Row-size diagnostics, value storage, and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `CHAR BYTE` | 🟡 | low | Alias behavior for `BINARY`, including metadata rewrites. | Parser and descriptor normalize `CHAR BYTE`, `CHAR(n) BYTE`, and `VARCHAR(n) BYTE` to binary/varbinary metadata for parse-only declarations. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `TINYBLOB` | 🟡 | medium | Binary large object length, metadata, and comparison semantics. | Internal descriptor records MySQL-compatible capacity and null charset/collation metadata; parse-only declarations are accepted. Storage/comparison semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `BLOB` | 🟡 | high | Binary large object length, metadata, and comparison semantics. | Internal descriptor maps optional `BLOB(M)` lengths to MySQL-compatible blob families and metadata; parse-only declarations are accepted. Storage/comparison semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `MEDIUMBLOB` | 🟡 | medium | Binary large object length, metadata, and comparison semantics. | Internal descriptor records MySQL-compatible capacity and null charset/collation metadata; parse-only declarations are accepted. Storage/comparison semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `LONGBLOB` | 🟡 | medium | Binary large object length, metadata, and comparison semantics. | Internal descriptor records MySQL-compatible capacity and null charset/collation metadata; parse-only declarations are accepted. Storage/comparison semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `TINYTEXT` | 🟡 | top | Text length, charset/collation, metadata, and index-prefix semantics. | Internal descriptor records MySQL-compatible capacity, charset/collation metadata, and binary-character-set normalization; parse-only declarations are accepted. Storage/comparison/index-prefix semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `TEXT` | 🟡 | top | Text length, charset/collation, metadata, and index-prefix semantics. | Internal descriptor maps optional `TEXT(M)` lengths by effective charset byte capacity, validates charset/collation clauses, and normalizes `CHARACTER SET binary` to blob metadata; parse-only declarations are accepted. Storage/comparison/index-prefix semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `MEDIUMTEXT` | 🟡 | top | Text length, charset/collation, metadata, and index-prefix semantics. | Internal descriptor records MySQL-compatible capacity and charset/collation metadata; parse-only declarations are accepted. Storage/comparison/index-prefix semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `LONGTEXT` | 🟡 | top | Text length, charset/collation, metadata, and index-prefix semantics. | Internal descriptor records MySQL-compatible capacity and charset/collation metadata; parse-only declarations are accepted. Storage/comparison/index-prefix semantics and executable DDL are deferred. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `LONG` / `LONG VARCHAR` | 🟡 | medium | Alias behavior for `MEDIUMTEXT`, including creation-time metadata rewrites. | Parser accepts `LONG VARCHAR` as parse-only `mediumtext`; storage and SHOW CREATE output are deferred. Other-vendor compatibility type. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `LONG VARBINARY` | 🟡 | medium | Alias behavior for `MEDIUMBLOB`, including creation-time metadata rewrites. | Parser accepts `LONG VARBINARY` as parse-only `mediumblob`; storage and SHOW CREATE output are deferred. Other-vendor compatibility type. See [string and binary column types spec](docs/specs/string-binary-column-types/specs.md). |
| `ENUM` | ❌ | high | Element indexing, sorting, invalid values, empty string, metadata, and DDL changes. |  |
| `SET` | ❌ | high | Bitmap membership, ordering, invalid values, metadata, and DDL changes. |  |
| `JSON` | ❌ | high | Binary JSON semantics, validation, comparison, partial update metadata, and generated-column interactions. |  |
| `GEOMETRY` | ❌ | medium | Base spatial type storage, SRID, validity, and metadata. |  |
| `POINT` | ❌ | medium | Point storage, SRID, coordinate access, and metadata. |  |
| `LINESTRING` | ❌ | medium | LineString storage, SRID, validity, and metadata. |  |
| `POLYGON` | ❌ | medium | Polygon storage, SRID, validity, and metadata. |  |
| `MULTIPOINT` | ❌ | medium | MultiPoint storage, SRID, validity, and metadata. |  |
| `MULTILINESTRING` | ❌ | medium | MultiLineString storage, SRID, validity, and metadata. |  |
| `MULTIPOLYGON` | ❌ | medium | MultiPolygon storage, SRID, validity, and metadata. |  |
| `GEOMETRYCOLLECTION` | ❌ | medium | Geometry collection storage, SRID, validity, and metadata. |  |
| Numeric literals | ❌ | top | Decimal, hex, bit, scientific notation, signedness, and overflow conversion. |  |
| String literals | ❌ | top | Escapes, introducers, national character set, binary literals, sql_mode effects, and concatenation. |  |
| Temporal literals | ❌ | top | DATE/TIME/TIMESTAMP literal syntax and coercion. |  |
| JSON path literals | ❌ | high | Path grammar, quoting, wildcards, ranges, and errors. |  |
| User variables | ❌ | high | Type retention, coercion, assignment, charset/collation, and result metadata. |  |
| Local variables | ❌ | medium | Stored-program variable typing, scope, and diagnostics. |  |
| Type conversion | ❌ | top | Expression, comparison, assignment, insert/update, aggregate, and function argument conversion rules. | Task 16 specifies the first reusable expression conversion subset for scalar operators, including NULL propagation, truthiness, numeric/string comparison conversion warnings, division-by-zero warnings, and unsigned bitwise behavior. Full assignment, aggregate, function, temporal, JSON, range-clipping, and collation coercion rules remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| Collation coercibility | ❌ | top | Coercibility ranks, illegal mix diagnostics, and result collation derivation. |  |

### 4.1 Character sets

| Character set | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `armscii8` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `ascii` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `big5` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `binary` | 🟡 | high | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. | Recognized in the internal registry with default collation `binary` for schema defaults and connection charset state; binary string comparison/conversion semantics and metadata listing are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `cp1250` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp1251` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp1256` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp1257` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp850` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp852` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp866` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `cp932` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `dec8` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `eucjpms` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `euckr` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `gb18030` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `gb2312` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `gbk` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `geostd8` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `greek` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `hebrew` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `hp8` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `keybcs2` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `koi8r` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `koi8u` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `latin1` | 🟡 | high | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. | Recognized in the internal registry with default collation `latin1_swedish_ci` and `latin1_bin` compatibility validation for schema defaults and connection charset state; conversion/comparison semantics and metadata listing are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `latin2` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `latin5` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `latin7` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `macce` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `macroman` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `sjis` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `swe7` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `tis620` | ❌ | low | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `ucs2` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `ujis` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf8mb3` | 🟡 | top | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. | Recognized in the internal registry with default collation `utf8mb3_general_ci` and `utf8mb3_bin` compatibility validation for schema defaults and connection charset state; conversion/comparison semantics and metadata listing are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `utf8mb4` | 🟡 | top | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. | Recognized in the internal registry with default collation `utf8mb4_0900_ai_ci` and `utf8mb4_bin` compatibility validation for schema defaults and connection charset state; conversion/comparison semantics and metadata listing are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `utf16` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf16le` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |
| `utf32` | ❌ | medium | Recognize, store, expose, and apply MySQL-compatible character set metadata and conversions. |  |


### 4.2 Collations and comparison behavior

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| Collation catalog entries | 🟡 | top | Expose every MySQL 8.4 collation through metadata, SHOW COLLATION, and charset/collation validation. | Internal registry validates `binary`, `latin1_swedish_ci`, `latin1_bin`, `utf8mb3_general_ci`, `utf8mb3_bin`, `utf8mb4_0900_ai_ci`, and `utf8mb4_bin`; full `SHOW COLLATION`/`INFORMATION_SCHEMA.COLLATIONS` exposure and all MySQL collations are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| Default collation selection | 🟡 | top | Match server, database, table, column, literal, and expression default collation resolution. | Implemented for server defaults, schema defaults, `SET NAMES`, `SET CHARACTER SET`, and schema DDL over the initial registry; table, column, literal, and expression default resolution are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| Unicode Collation Algorithm families | ❌ | top | Match utf8mb4_0900, unicode, language-specific, accent-sensitive, case-sensitive, and kana-sensitive behavior where MySQL exposes it. |  |
| Binary collations | 🟡 | top | Match binary string ordering, equality, case sensitivity, and metadata flags. | `binary`, `latin1_bin`, `utf8mb3_bin`, and `utf8mb4_bin` are recognized for validation and session/schema defaults; comparison, ordering, and metadata flags are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| PAD SPACE and NO PAD collations | ❌ | top | Match trailing-space comparison, uniqueness, ordering, and index behavior. |  |
| Collation coercibility rules | ❌ | top | Match coercibility ranks, illegal mix diagnostics, and result collation derivation. |  |
| WEIGHT_STRING behavior | ❌ | medium | Match sort weight generation for supported collations and strings. |  |

## 5. Built-in functions and operators

This table is generated from the MySQL 8.4 built-in function and operator reference. Internal-only entries are listed because applications and metadata queries can still observe their names or diagnostics.

| Function or operator | Status | Priority | MySQL behavior to match | Implementation notes |
| --- | --- | --- | --- | --- |
| `&` | 🟡 | top | Bitwise AND | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `>` | 🟡 | top | Greater than operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `>>` | 🟡 | medium | Right shift | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `>=` | 🟡 | top | Greater than or equal operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `<` | 🟡 | top | Less than operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `<>, !=` | 🟡 | top | Not equal operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `<<` | 🟡 | medium | Left shift | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `<=` | 🟡 | top | Less than or equal operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `<=>` | 🟡 | top | NULL-safe equal to operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Task 29 also implements uncorrelated multi-element row scalar subquery comparison with null-safe row-subquery truth semantics and `NOT_NULL` metadata in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY` contexts. MyLite rejects `<=> ANY` syntax in the quantified subquery grammar. Broader row expression contexts, full SQL-mode variants, complete collation behavior, functions, casts, correlation, DML contexts, and general row quantified comparisons remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md) and [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md). |
| `%, MOD` | 🟡 | top | Modulo operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `*` | 🟡 | top | Multiplication operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `+` | 🟡 | top | Addition operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `-` (binary) | 🟡 | top | Minus operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `-` (unary) | 🟡 | top | Change the sign of the argument | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `->` | ❌ | high | Return value from JSON column after evaluating path; equivalent to JSON_EXTRACT(). |  |
| `->>` | ❌ | high | Return value from JSON column after evaluating path and unquoting the result; equivalent to JSON_UNQUOTE(JSON_EXTRACT()). |  |
| `/` | 🟡 | top | Division operator | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `:=` | ❌ | top | Assign a value |  |
| `=` (assignment) | ❌ | top | Assign a value as part of `SET` or an `UPDATE` assignment. |  |
| `=` (comparison) | 🟡 | top | Equal operator | Implemented for no-table scalar `SELECT` comparisons. Assignment `=` remains separate; row expression contexts outside single-table SELECT `WHERE`, full metadata, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `^` | 🟡 | top | Bitwise XOR | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `ABS()` | 🟡 | high | Return the absolute value | Implemented in the Task 24 pure scalar function subset for existing scalar expression call sites. Exact overflow diagnostics and full decimal metadata remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `ACOS()` | ❌ | medium | Return the arc cosine |  |
| `ADDDATE()` | ❌ | top | Add time values (intervals) to a date value |  |
| `ADDTIME()` | ❌ | medium | Add time |  |
| `AES_DECRYPT()` | ❌ | medium | Decrypt using AES |  |
| `AES_ENCRYPT()` | ❌ | medium | Encrypt using AES |  |
| `AND, &&` | 🟡 | top | Logical AND | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `ANY` / `SOME` / `ALL` quantified subquery comparisons | 🟡 | high | Compare a scalar operand with a one-column subquery using existential or universal three-valued truth semantics; support MySQL-accepted row quantified aliases as row membership. | Implemented for uncorrelated scalar left operands, one-column subqueries, operators `=`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`, no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY` contexts, MySQL truth semantics for empty and `NULL` results, warning-preserving short-circuit evaluation, error 1235 for inner `LIMIT`, error 1241 for multi-column output, syntax rejection for `<=> ANY`, and nullable boolean metadata. Also implemented for the MySQL-accepted row aliases row `= ANY`, row `= SOME`, row `<> ALL`, and row `!= ALL` by reusing row `IN` / `NOT IN` semantics, width validation, warning behavior, inner `ORDER BY` suppression, and metadata. General row quantified comparisons, row `<=> ANY`, correlation, DML contexts, and broader query-expression surfaces remain deferred. See [quantified subquery comparisons spec](docs/specs/quantified-subquery-comparisons/specs.md) and [row quantified subquery comparisons spec](docs/specs/row-quantified-subquery-comparisons/specs.md). |
| `ANY_VALUE()` | ❌ | medium | Suppress ONLY_FULL_GROUP_BY value rejection |  |
| `ASCII()` | ❌ | high | Return numeric value of left-most character |  |
| `ASIN()` | ❌ | medium | Return the arc sine |  |
| `asynchronous_connection_failover_add_managed()` | ❌ | low | Add group member source server configuration information to a replication channel source list |  |
| `asynchronous_connection_failover_add_source()` | ❌ | low | Add source server configuration information server to a replication channel source list |  |
| `asynchronous_connection_failover_delete_managed()` | ❌ | low | Remove a managed group from a replication channel source list |  |
| `asynchronous_connection_failover_delete_source()` | ❌ | low | Remove a source server from a replication channel source list |  |
| `asynchronous_connection_failover_reset()` | ❌ | low | Remove all settings relating to group replication asynchronous failover |  |
| `ATAN()` | ❌ | medium | Return the arc tangent |  |
| `ATAN2(), ATAN()` | ❌ | medium | Return the arc tangent of the two arguments |  |
| `AVG()` | 🟡 | top | Return the average value of the argument | Implemented for the first single-table aggregate slice, including implicit and grouped aggregates, NULL skipping, empty-input `NULL`, numeric conversion warnings, no-table top-level aggregate calls, metadata, `HAVING`, and `ORDER BY`. `DISTINCT`, window execution, native fixed-point arithmetic, joins, subqueries, and broad type/collation fidelity remain deferred. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| `BENCHMARK()` | ❌ | low | Repeatedly execute an expression |  |
| `BETWEEN ... AND ...` | 🟡 | top | Whether a value is within a range of values | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `BIN()` | ❌ | high | Return a string containing binary representation of a number |  |
| `BIN_TO_UUID()` | ❌ | high | Convert binary UUID to string |  |
| `BINARY` | ❌ | medium | Cast a string to a binary string |  |
| `BIT_AND()` | ❌ | medium | Return bitwise AND |  |
| `BIT_COUNT()` | ❌ | medium | Return the number of bits that are set |  |
| `BIT_LENGTH()` | ❌ | medium | Return length of argument in bits |  |
| `BIT_OR()` | ❌ | medium | Return bitwise OR |  |
| `BIT_XOR()` | ❌ | medium | Return bitwise XOR |  |
| `CAN_ACCESS_COLUMN()` | ❌ | low | Internal use only |  |
| `CAN_ACCESS_DATABASE()` | ❌ | low | Internal use only |  |
| `CAN_ACCESS_TABLE()` | ❌ | low | Internal use only |  |
| `CAN_ACCESS_USER()` | ❌ | low | Internal use only |  |
| `CAN_ACCESS_VIEW()` | ❌ | low | Internal use only |  |
| `CASE` | 🟡 | top | Simple and searched SQL CASE expression semantics, short-circuit evaluation, metadata, warnings/errors, and supported scalar expression contexts. | Implemented for SQL CASE expressions, distinct from stored-program `CASE` statements, including parser/AST support, simple and searched runtime semantics, short-circuit evaluation, result metadata aggregation, evaluated-branch warnings/errors, binding of unselected branches, no-table `SELECT`, one-table `SELECT` projection/`WHERE`/`ORDER BY`, `UPDATE` assignment/predicate/order, and `DELETE` predicate/order. `INSERT` expression paths, stored-program `CASE` statements, exact collation-sensitive comparison, and exhaustive mixed-type metadata remain deferred. See [CASE expression spec](docs/specs/case-expression/specs.md). |
| `CAST()` | 🟡 | top | Cast a value as a certain type | Implemented for the Task 26 MySQL-compatible expression subset: signed/unsigned integer, DECIMAL/DEC, CHAR/NCHAR, and BINARY metadata casts across supported scalar expression call sites, with parser/AST support, runtime results, warnings, metadata, and DML strictness tests. `CONVERT()`, temporal/JSON/spatial casts, fixed-length binary padding, `AT TIME ZONE`, native fixed-point arithmetic, and exhaustive overflow/SQL-mode behavior remain deferred. See [CAST expression spec](docs/specs/cast-expression/specs.md). |
| `CEIL()` | 🟡 | high | Return the smallest integer value not less than the argument | Implemented in the Task 24 pure scalar function subset for existing scalar expression call sites. Full type coercion edge cases remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `CEILING()` | 🟡 | high | Return the smallest integer value not less than the argument | Implemented as a `CEIL()` synonym in the Task 24 pure scalar function subset. Full type coercion edge cases remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `CHAR()` | ❌ | high | Return the character for each integer passed |  |
| `CHAR_LENGTH()` | 🟡 | top | Return number of characters in argument | Implemented in the Task 24 pure scalar function subset with UTF-8 character counting for current text values. Full charset/collation semantics remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `CHARACTER_LENGTH()` | 🟡 | top | Synonym for CHAR_LENGTH() | Implemented as a `CHAR_LENGTH()` synonym in the Task 24 pure scalar function subset. Full charset/collation semantics remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `CHARSET()` | ❌ | high | Return the character set of the argument |  |
| `COALESCE()` | 🟡 | top | Return the first non-NULL argument | Implemented in the Task 24 pure scalar function subset with short-circuit evaluation. Full type aggregation metadata remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `COERCIBILITY()` | ❌ | high | Return the collation coercibility value of the string argument |  |
| `COLLATION()` | ❌ | high | Return the collation of the string argument |  |
| `COMPRESS()` | ❌ | medium | Return result as a binary string |  |
| `CONCAT()` | 🟡 | top | Return concatenated string | Implemented in the Task 24 pure scalar function subset, including NULL propagation and result metadata for covered cases. Full collation/coercibility and exact arity diagnostics remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `CONCAT_WS()` | ❌ | top | Return concatenate with separator |  |
| `CONNECTION_ID()` | ❌ | high | Return the connection ID (thread ID) for the connection |  |
| `CONV()` | ❌ | high | Convert numbers between different number bases |  |
| `CONVERT()` | ❌ | high | Cast a value as a certain type |  |
| `CONVERT_TZ()` | ❌ | medium | Convert from one time zone to another |  |
| `COS()` | ❌ | medium | Return the cosine |  |
| `COT()` | ❌ | medium | Return the cotangent |  |
| `COUNT()` | 🟡 | top | Return a count of the number of rows returned | Implemented for the first single-table aggregate slice, including `COUNT(*)`, `COUNT(expr)`, implicit and grouped aggregates, empty-input zero, no-table top-level aggregate calls, metadata, `HAVING`, `ORDER BY`, and diagnostics for invalid placement/arity. `COUNT(DISTINCT)`, window execution, joins, subqueries, and optimizer pushdown remain deferred. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| `COUNT(DISTINCT)` | ❌ | top | Return the count of a number of different values | Deferred from the first aggregate slice pending duplicate-elimination semantics; MySQL-verified behavior is recorded in the aggregate grouping spec. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| `CRC32()` | ❌ | high | Compute a cyclic redundancy check value |  |
| `CUME_DIST()` | ❌ | medium | Cumulative distribution value |  |
| `CURDATE()` | ❌ | top | Return the current date |  |
| `CURRENT_DATE(), CURRENT_DATE` | ❌ | top | Synonyms for CURDATE() |  |
| `CURRENT_ROLE()` | ❌ | high | Return the current active roles |  |
| `CURRENT_TIME(), CURRENT_TIME` | ❌ | top | Synonyms for CURTIME() |  |
| `CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP` | ❌ | top | Synonyms for NOW() |  |
| `CURRENT_USER(), CURRENT_USER` | ❌ | high | The authenticated user name and host name |  |
| `CURTIME()` | ❌ | top | Return the current time |  |
| `DATABASE()` | ❌ | top | Return the default (current) database name |  |
| `DATE()` | ❌ | top | Extract the date part of a date or datetime expression |  |
| `DATE_ADD()` | ❌ | top | Add time values (intervals) to a date value |  |
| `DATE_FORMAT()` | ❌ | top | Format date as specified |  |
| `DATE_SUB()` | ❌ | top | Subtract a time value (interval) from a date |  |
| `DATEDIFF()` | ❌ | top | Subtract two dates |  |
| `DAY()` | ❌ | high | Synonym for DAYOFMONTH() |  |
| `DAYNAME()` | ❌ | medium | Return the name of the weekday |  |
| `DAYOFMONTH()` | ❌ | high | Return the day of the month (0-31) |  |
| `DAYOFWEEK()` | ❌ | high | Return the weekday index of the argument |  |
| `DAYOFYEAR()` | ❌ | high | Return the day of the year (1-366) |  |
| `DEFAULT()` | ❌ | top | Return the default value for a table column |  |
| `DEGREES()` | ❌ | medium | Convert radians to degrees |  |
| `DENSE_RANK()` | ❌ | medium | Rank of current row within its partition, without gaps |  |
| `DIV` | 🟡 | medium | Integer division | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `ELT()` | ❌ | high | Return string at index number |  |
| `EXISTS()` | 🟡 | top | Whether the result of a query contains any rows | Implemented for Task 29's first executable subquery slice: uncorrelated `EXISTS` in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, `ON`, `HAVING`, and `ORDER BY` contexts. Correlation, DML contexts, optimizer transformations, and broader subquery row-source coverage remain deferred. See [subqueries spec](docs/specs/subqueries/specs.md). |
| `EXP()` | ❌ | medium | Raise to the power of |  |
| `EXPORT_SET()` | ❌ | medium | Return a string such that for every bit set in the value bits, you get an on string and for every unset bit, you get an off string |  |
| `EXTRACT()` | ❌ | high | Extract part of a date |  |
| `ExtractValue()` | ❌ | low | Extract a value from an XML string using XPath notation |  |
| `FIELD()` | ❌ | high | Index (position) of first argument in subsequent arguments |  |
| `FIND_IN_SET()` | ❌ | high | Index (position) of first argument within second argument |  |
| `FIRST_VALUE()` | ❌ | medium | Value of argument from first row of window frame |  |
| `FLOOR()` | 🟡 | high | Return the largest integer value not greater than the argument | Implemented in the Task 24 pure scalar function subset for existing scalar expression call sites. Full type coercion edge cases remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `FORMAT()` | ❌ | high | Return a number formatted to specified number of decimal places |  |
| `FORMAT_BYTES()` | ❌ | medium | Convert byte count to value with units |  |
| `FORMAT_PICO_TIME()` | ❌ | medium | Convert time in picoseconds to value with units |  |
| `FOUND_ROWS()` | ❌ | top | For a SELECT with a LIMIT clause, the number of rows that would be returned were there no LIMIT clause |  |
| `FROM_BASE64()` | ❌ | medium | Decode base64 encoded string and return result |  |
| `FROM_DAYS()` | ❌ | medium | Convert a day number to a date |  |
| `FROM_UNIXTIME()` | ❌ | top | Format Unix timestamp as a date |  |
| `GeomCollection()` | ❌ | medium | Construct geometry collection from geometries |  |
| `GeometryCollection()` | ❌ | medium | Construct geometry collection from geometries |  |
| `GET_DD_COLUMN_PRIVILEGES()` | ❌ | low | Internal use only |  |
| `GET_DD_CREATE_OPTIONS()` | ❌ | low | Internal use only |  |
| `GET_DD_INDEX_SUB_PART_LENGTH()` | ❌ | low | Internal use only |  |
| `GET_FORMAT()` | ❌ | medium | Return a date format string |  |
| `GET_LOCK()` | ❌ | high | Get a named lock |  |
| `GREATEST()` | ❌ | medium | Return the largest argument |  |
| `GROUP_CONCAT()` | ❌ | top | Return a concatenated string |  |
| `group_replication_disable_member_action()` | ❌ | low | Disable member action for event specified |  |
| `group_replication_enable_member_action()` | ❌ | low | Enable member action for event specified |  |
| `group_replication_get_communication_protocol()` | ❌ | low | Get version of group replication communication protocol currently in use |  |
| `group_replication_get_write_concurrency()` | ❌ | low | Get maximum number of consensus instances currently set for group |  |
| `group_replication_reset_member_actions()` | ❌ | low | Reset all member actions to defaults and configuration version number to 1 |  |
| `group_replication_set_as_primary()` | ❌ | low | Make a specific group member the primary |  |
| `group_replication_set_communication_protocol()` | ❌ | low | Set version for group replication communication protocol to use |  |
| `group_replication_set_write_concurrency()` | ❌ | low | Set maximum number of consensus instances that can be executed in parallel |  |
| `group_replication_switch_to_multi_primary_mode()` | ❌ | low | Changes the mode of a group running in single-primary mode to multi-primary mode |  |
| `group_replication_switch_to_single_primary_mode()` | ❌ | low | Changes the mode of a group running in multi-primary mode to single-primary mode |  |
| `GROUPING()` | ❌ | medium | Distinguish super-aggregate ROLLUP rows from regular rows |  |
| `HEX()` | ❌ | high | Hexadecimal representation of decimal or string value |  |
| `HOUR()` | ❌ | high | Extract the hour |  |
| `ICU_VERSION()` | ❌ | medium | ICU library version |  |
| `IF()` | 🟡 | top | If/else construct | Implemented in the Task 24 pure scalar function subset with three-argument syntax and branch short-circuit evaluation. Full type aggregation metadata remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `IFNULL()` | 🟡 | top | Null if/else construct | Implemented in the Task 24 pure scalar function subset with fallback short-circuit evaluation. Full type aggregation metadata remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `IN()` | 🟡 | top | Whether a value is within a set of values | Implemented for expression-list predicates in no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Task 29 also implements uncorrelated scalar `IN` subqueries and multi-element row `IN` subqueries in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY` contexts. Correlated subqueries, DML contexts, full SQL-mode variants, complete collation behavior, and broad function/cast interactions remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), and [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md). |
| `INET_ATON()` | ❌ | high | Return the numeric value of an IP address |  |
| `INET_NTOA()` | ❌ | high | Return the IP address from a numeric value |  |
| `INSERT()` | ❌ | high | Insert substring at specified position up to specified number of characters |  |
| `INSTR()` | ❌ | high | Return the index of the first occurrence of substring |  |
| `INTERNAL_AUTO_INCREMENT()` | ❌ | low | Internal use only |  |
| `INTERNAL_AVG_ROW_LENGTH()` | ❌ | low | Internal use only |  |
| `INTERNAL_CHECK_TIME()` | ❌ | low | Internal use only |  |
| `INTERNAL_CHECKSUM()` | ❌ | low | Internal use only |  |
| `INTERNAL_DATA_FREE()` | ❌ | low | Internal use only |  |
| `INTERNAL_DATA_LENGTH()` | ❌ | low | Internal use only |  |
| `INTERNAL_DD_CHAR_LENGTH()` | ❌ | low | Internal use only |  |
| `INTERNAL_GET_COMMENT_OR_ERROR()` | ❌ | low | Internal use only |  |
| `INTERNAL_GET_ENABLED_ROLE_JSON()` | ❌ | low | Internal use only |  |
| `INTERNAL_GET_HOSTNAME()` | ❌ | low | Internal use only |  |
| `INTERNAL_GET_USERNAME()` | ❌ | low | Internal use only |  |
| `INTERNAL_GET_VIEW_WARNING_OR_ERROR()` | ❌ | low | Internal use only |  |
| `INTERNAL_INDEX_COLUMN_CARDINALITY()` | ❌ | low | Internal use only |  |
| `INTERNAL_INDEX_LENGTH()` | ❌ | low | Internal use only |  |
| `INTERNAL_IS_ENABLED_ROLE()` | ❌ | low | Internal use only |  |
| `INTERNAL_IS_MANDATORY_ROLE()` | ❌ | low | Internal use only |  |
| `INTERNAL_KEYS_DISABLED()` | ❌ | low | Internal use only |  |
| `INTERNAL_MAX_DATA_LENGTH()` | ❌ | low | Internal use only |  |
| `INTERNAL_TABLE_ROWS()` | ❌ | low | Internal use only |  |
| `INTERNAL_UPDATE_TIME()` | ❌ | low | Internal use only |  |
| `INTERVAL()` | ❌ | medium | Return the index of the argument that is less than the first argument |  |
| `IS` | 🟡 | top | Test a value against a boolean | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `IS_FREE_LOCK()` | ❌ | high | Whether the named lock is free |  |
| `IS NOT` | 🟡 | top | Test a value against a boolean | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `IS NOT NULL` | 🟡 | top | NOT NULL value test | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `IS NULL` | 🟡 | top | NULL value test | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `IS_USED_LOCK()` | ❌ | high | Whether the named lock is in use; return connection identifier if true |  |
| `IS_UUID()` | ❌ | medium | Whether argument is a valid UUID |  |
| `ISNULL()` | 🟡 | top | Test whether the argument is NULL | Implemented in the Task 24 pure scalar function subset for existing scalar expression call sites. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `JSON_ARRAY()` | ❌ | high | Create JSON array |  |
| `JSON_ARRAY_APPEND()` | ❌ | medium | Append data to JSON document |  |
| `JSON_ARRAY_INSERT()` | ❌ | medium | Insert into JSON array |  |
| `JSON_ARRAYAGG()` | ❌ | high | Return result set as a single JSON array |  |
| `JSON_CONTAINS()` | ❌ | high | Whether JSON document contains specific object at path |  |
| `JSON_CONTAINS_PATH()` | ❌ | high | Whether JSON document contains any data at path |  |
| `JSON_DEPTH()` | ❌ | high | Maximum depth of JSON document |  |
| `JSON_EXTRACT()` | ❌ | high | Return data from JSON document |  |
| `JSON_INSERT()` | ❌ | high | Insert data into JSON document |  |
| `JSON_KEYS()` | ❌ | high | Array of keys from JSON document |  |
| `JSON_LENGTH()` | ❌ | high | Number of elements in JSON document |  |
| `JSON_MERGE()` | ❌ | high | Merge JSON documents, preserving duplicate keys. Deprecated synonym for JSON_MERGE_PRESERVE() |  |
| `JSON_MERGE_PATCH()` | ❌ | high | Merge JSON documents, replacing values of duplicate keys |  |
| `JSON_MERGE_PRESERVE()` | ❌ | high | Merge JSON documents, preserving duplicate keys |  |
| `JSON_OBJECT()` | ❌ | high | Create JSON object |  |
| `JSON_OBJECTAGG()` | ❌ | high | Return result set as a single JSON object |  |
| `JSON_OVERLAPS()` | ❌ | high | Compares two JSON documents, returns TRUE (1) if these have any key-value pairs or array elements in common, otherwise FALSE (0) |  |
| `JSON_PRETTY()` | ❌ | high | Print a JSON document in human-readable format |  |
| `JSON_QUOTE()` | ❌ | high | Quote JSON document |  |
| `JSON_REMOVE()` | ❌ | high | Remove data from JSON document |  |
| `JSON_REPLACE()` | ❌ | high | Replace values in JSON document |  |
| `JSON_SCHEMA_VALID()` | ❌ | medium | Validate JSON document against JSON schema; returns TRUE/1 if document validates against schema, or FALSE/0 if it does not |  |
| `JSON_SCHEMA_VALIDATION_REPORT()` | ❌ | medium | Validate JSON document against JSON schema; returns report in JSON format on outcome on validation including success or failure and reasons for failure |  |
| `JSON_SEARCH()` | ❌ | high | Path to value within JSON document |  |
| `JSON_SET()` | ❌ | high | Insert data into JSON document |  |
| `JSON_STORAGE_FREE()` | ❌ | medium | Freed space within binary representation of JSON column value following partial update |  |
| `JSON_STORAGE_SIZE()` | ❌ | medium | Space used for storage of binary representation of a JSON document |  |
| `JSON_TABLE()` | ❌ | medium | Return data from a JSON expression as a relational table |  |
| `JSON_TYPE()` | ❌ | high | Type of JSON value |  |
| `JSON_UNQUOTE()` | ❌ | high | Unquote JSON value |  |
| `JSON_VALID()` | ❌ | high | Whether JSON value is valid |  |
| `JSON_VALUE()` | ❌ | high | Extract value from JSON document at location pointed to by path provided; return this value as VARCHAR(512) or specified type |  |
| `LAG()` | ❌ | medium | Value of argument from row lagging current row within partition |  |
| `LAST_DAY` | ❌ | high | Return the last day of the month for the argument |  |
| `LAST_INSERT_ID()` | ❌ | top | Value of the AUTOINCREMENT column for the last INSERT |  |
| `LAST_VALUE()` | ❌ | medium | Value of argument from last row of window frame |  |
| `LCASE()` | 🟡 | top | Synonym for LOWER() | Implemented as a `LOWER()` synonym in the Task 24 pure scalar function subset. Non-ASCII case folding remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `LEAD()` | ❌ | medium | Value of argument from row leading current row within partition |  |
| `LEAST()` | ❌ | medium | Return the smallest argument |  |
| `LEFT()` | 🟡 | high | Return the leftmost number of characters as specified | Implemented in the Task 24 pure scalar function subset, including zero/negative counts and UTF-8 character offsets. Full charset/collation semantics remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `LENGTH()` | 🟡 | top | Return the length of a string in bytes | Implemented in the Task 24 pure scalar function subset. Full charset/collation semantics remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `LIKE` | 🟡 | top | Simple pattern matching | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `LineString()` | ❌ | medium | Construct LineString from Point values |  |
| `LN()` | ❌ | medium | Return the natural logarithm of the argument |  |
| `LOAD_FILE()` | ❌ | medium | Load the named file |  |
| `LOCALTIME(), LOCALTIME` | ❌ | top | Synonym for NOW() |  |
| `LOCALTIMESTAMP, LOCALTIMESTAMP()` | ❌ | top | Synonym for NOW() |  |
| `LOCATE()` | ❌ | high | Return the position of the first occurrence of substring |  |
| `LOG()` | ❌ | medium | Return the natural logarithm of the first argument |  |
| `LOG10()` | ❌ | medium | Return the base-10 logarithm of the argument |  |
| `LOG2()` | ❌ | medium | Return the base-2 logarithm of the argument |  |
| `LOWER()` | 🟡 | top | Return the argument in lowercase | Implemented in the Task 24 pure scalar function subset with ASCII case mapping. Non-ASCII case folding remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `LPAD()` | ❌ | medium | Return the string argument, left-padded with the specified string |  |
| `LTRIM()` | ❌ | top | Remove leading spaces |  |
| `MAKE_SET()` | ❌ | high | Return a set of comma-separated strings that have the corresponding bit in bits set |  |
| `MAKEDATE()` | ❌ | medium | Create a date from the year and day of year |  |
| `MAKETIME()` | ❌ | medium | Create time from hour, minute, second |  |
| `MASTER_POS_WAIT()` | ❌ | low | Block until the replica has read and applied all updates up to the specified position |  |
| `MATCH()` | ❌ | high | Perform full-text search |  |
| `MAX()` | 🟡 | top | Return the maximum value | Implemented for the first single-table aggregate slice, including implicit and grouped aggregates, NULL skipping, empty-input `NULL`, current string/numeric comparison domains, no-table top-level aggregate calls, metadata, `HAVING`, and `ORDER BY`. `DISTINCT`, window execution, joins, subqueries, full collation semantics, and broad type fidelity remain deferred. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| `MBRContains()` | ❌ | medium | Whether MBR of one geometry contains MBR of another |  |
| `MBRCoveredBy()` | ❌ | medium | Whether one MBR is covered by another |  |
| `MBRCovers()` | ❌ | medium | Whether one MBR covers another |  |
| `MBRDisjoint()` | ❌ | medium | Whether MBRs of two geometries are disjoint |  |
| `MBREquals()` | ❌ | medium | Whether MBRs of two geometries are equal |  |
| `MBRIntersects()` | ❌ | medium | Whether MBRs of two geometries intersect |  |
| `MBROverlaps()` | ❌ | medium | Whether MBRs of two geometries overlap |  |
| `MBRTouches()` | ❌ | medium | Whether MBRs of two geometries touch |  |
| `MBRWithin()` | ❌ | medium | Whether MBR of one geometry is within MBR of another |  |
| `MD5()` | ❌ | high | Calculate MD5 checksum |  |
| `MEMBER OF()` | ❌ | high | Returns true (1) if first operand matches any element of JSON array passed as second operand, otherwise returns false (0) |  |
| `MICROSECOND()` | ❌ | high | Return the microseconds from argument |  |
| `MID()` | ❌ | top | Return a substring starting from the specified position |  |
| `MIN()` | 🟡 | top | Return the minimum value | Implemented for the first single-table aggregate slice, including implicit and grouped aggregates, NULL skipping, empty-input `NULL`, current string/numeric comparison domains, no-table top-level aggregate calls, metadata, `HAVING`, and `ORDER BY`. `DISTINCT`, window execution, joins, subqueries, full collation semantics, and broad type fidelity remain deferred. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| `MINUTE()` | ❌ | high | Return the minute from the argument |  |
| `MOD()` | 🟡 | top | Return the remainder | Implemented as an operator in Task 16 and as a function in the Task 24 pure scalar function subset, including division-by-zero warning behavior in covered scalar results. Full type coercion edge cases remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md) and [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `MONTH()` | ❌ | high | Return the month from the date passed |  |
| `MONTHNAME()` | ❌ | medium | Return the name of the month |  |
| `MultiLineString()` | ❌ | medium | Contruct MultiLineString from LineString values |  |
| `MultiPoint()` | ❌ | medium | Construct MultiPoint from Point values |  |
| `MultiPolygon()` | ❌ | medium | Construct MultiPolygon from Polygon values |  |
| `NAME_CONST()` | ❌ | medium | Cause the column to have the given name |  |
| `NOT, !` | 🟡 | top | Negates value | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `NOT BETWEEN ... AND ...` | 🟡 | top | Whether a value is not within a range of values | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `NOT EXISTS()` | 🟡 | top | Whether the result of a query contains no rows | Implemented for Task 29's first executable subquery slice: uncorrelated `NOT EXISTS` in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, `ON`, `HAVING`, and `ORDER BY` contexts. Correlation, DML contexts, optimizer transformations, and broader subquery row-source coverage remain deferred. See [subqueries spec](docs/specs/subqueries/specs.md). |
| `NOT IN()` | 🟡 | top | Whether a value is not within a set of values | Implemented for expression-list predicates in no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Task 29 also implements uncorrelated scalar `NOT IN` subqueries and multi-element row `NOT IN` subqueries in no-table scalar `SELECT` and current table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY` contexts. Correlated subqueries, DML contexts, full SQL-mode variants, complete collation behavior, and broad function/cast interactions remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md), [subquery `IN` predicates spec](docs/specs/subquery-in-predicates/specs.md), and [row subquery predicates spec](docs/specs/row-subquery-predicates/specs.md). |
| `NOT LIKE` | 🟡 | top | Negation of simple pattern matching | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `NOT REGEXP` | ❌ | high | Negation of REGEXP |  |
| `NOW()` | ❌ | top | Return the current date and time |  |
| `NTH_VALUE()` | ❌ | medium | Value of argument from N-th row of window frame |  |
| `NTILE()` | ❌ | medium | Bucket number of current row within its partition. |  |
| `NULLIF()` | 🟡 | top | Return NULL if expr1 = expr2 | Implemented in the Task 24 pure scalar function subset for existing scalar expression call sites. Full type aggregation metadata and MySQL's repeated-evaluation edge behavior remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `OCT()` | ❌ | high | Return a string containing octal representation of a number |  |
| `OCTET_LENGTH()` | 🟡 | top | Synonym for LENGTH() | Implemented as a `LENGTH()` synonym in the Task 24 pure scalar function subset. Full charset/collation semantics remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `OR, \|\|` | 🟡 | top | Logical OR | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `ORD()` | ❌ | medium | Return character code for leftmost character of the argument |  |
| `PERCENT_RANK()` | ❌ | medium | Percentage rank value |  |
| `PERIOD_ADD()` | ❌ | medium | Add a period to a year-month |  |
| `PERIOD_DIFF()` | ❌ | medium | Return the number of months between periods |  |
| `PI()` | 🟡 | medium | Return the value of pi | Implemented in the Task 24 pure scalar function subset with MySQL-compatible display text and metadata for covered scalar results. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `Point()` | ❌ | medium | Construct Point from coordinates |  |
| `Polygon()` | ❌ | medium | Construct Polygon from LineString arguments |  |
| `POSITION()` | ❌ | high | Synonym for LOCATE() |  |
| `POW()` | ❌ | medium | Return the argument raised to the specified power |  |
| `POWER()` | ❌ | medium | Return the argument raised to the specified power |  |
| `PS_CURRENT_THREAD_ID()` | ❌ | low | Performance Schema thread ID for current thread |  |
| `PS_THREAD_ID()` | ❌ | low | Performance Schema thread ID for given thread |  |
| `QUARTER()` | ❌ | high | Return the quarter from a date argument |  |
| `QUOTE()` | ❌ | high | Escape the argument for use in an SQL statement |  |
| `RADIANS()` | ❌ | medium | Return argument converted to radians |  |
| `RAND()` | ❌ | high | Return a random floating-point value |  |
| `RANDOM_BYTES()` | ❌ | medium | Return a random byte vector |  |
| `RANK()` | ❌ | medium | Rank of current row within its partition, with gaps |  |
| `REGEXP` | ❌ | high | Whether string matches regular expression |  |
| `REGEXP_INSTR()` | ❌ | high | Starting index of substring matching regular expression |  |
| `REGEXP_LIKE()` | ❌ | high | Whether string matches regular expression |  |
| `REGEXP_REPLACE()` | ❌ | high | Replace substrings matching regular expression |  |
| `REGEXP_SUBSTR()` | ❌ | high | Return substring matching regular expression |  |
| `RELEASE_ALL_LOCKS()` | ❌ | high | Release all current named locks |  |
| `RELEASE_LOCK()` | ❌ | high | Release the named lock |  |
| `REPEAT()` | ❌ | high | Repeat a string the specified number of times |  |
| `REPLACE()` | 🟡 | top | Replace occurrences of a specified string | Implemented in the Task 24 pure scalar function subset, including NULL propagation and empty search-string behavior. Full collation-sensitive matching remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `REVERSE()` | ❌ | high | Reverse the characters in a string |  |
| `RIGHT()` | 🟡 | high | Return the specified rightmost number of characters | Implemented in the Task 24 pure scalar function subset, including zero/negative counts and UTF-8 character offsets. Full charset/collation semantics remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `RLIKE` | ❌ | high | Whether string matches regular expression |  |
| `ROLES_GRAPHML()` | ❌ | low | Return a GraphML document representing memory role subgraphs |  |
| `ROUND()` | ❌ | high | Round the argument |  |
| `ROW_COUNT()` | ❌ | top | The number of rows updated |  |
| `ROW_NUMBER()` | ❌ | medium | Number of current row within its partition |  |
| `RPAD()` | ❌ | medium | Append string the specified number of times |  |
| `RTRIM()` | ❌ | top | Remove trailing spaces |  |
| `SCHEMA()` | ❌ | top | Synonym for DATABASE() |  |
| `SEC_TO_TIME()` | ❌ | high | Converts seconds to 'hh:mm:ss' format |  |
| `SECOND()` | ❌ | high | Return the second (0-59) |  |
| `SESSION_USER()` | ❌ | high | Synonym for USER() |  |
| `SHA1(), SHA()` | ❌ | high | Calculate an SHA-1 160-bit checksum |  |
| `SHA2()` | ❌ | high | Calculate an SHA-2 checksum |  |
| `SIGN()` | 🟡 | medium | Return the sign of the argument | Implemented in the Task 24 pure scalar function subset for existing scalar expression call sites. Full type coercion edge cases remain deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `SIN()` | ❌ | medium | Return the sine of the argument |  |
| `SLEEP()` | ❌ | low | Sleep for a number of seconds |  |
| `SOUNDEX()` | ❌ | medium | Return a soundex string |  |
| `SOUNDS LIKE` | ❌ | medium | Compare sounds |  |
| `SOURCE_POS_WAIT()` | ❌ | low | Block until the replica has read and applied all updates up to the specified position |  |
| `SPACE()` | ❌ | high | Return a string of the specified number of spaces |  |
| `SQRT()` | ❌ | medium | Return the square root of the argument |  |
| `ST_Area()` | ❌ | medium | Return Polygon or MultiPolygon area |  |
| `ST_AsBinary(), ST_AsWKB()` | ❌ | medium | Convert from internal geometry format to WKB |  |
| `ST_AsGeoJSON()` | ❌ | medium | Generate GeoJSON object from geometry |  |
| `ST_AsText(), ST_AsWKT()` | ❌ | medium | Convert from internal geometry format to WKT |  |
| `ST_Buffer()` | ❌ | medium | Return geometry of points within given distance from geometry |  |
| `ST_Buffer_Strategy()` | ❌ | medium | Produce strategy option for ST_Buffer() |  |
| `ST_Centroid()` | ❌ | medium | Return centroid as a point |  |
| `ST_Collect()` | ❌ | medium | Aggregate spatial values into collection |  |
| `ST_Contains()` | ❌ | medium | Whether one geometry contains another |  |
| `ST_ConvexHull()` | ❌ | medium | Return convex hull of geometry |  |
| `ST_Crosses()` | ❌ | medium | Whether one geometry crosses another |  |
| `ST_Difference()` | ❌ | medium | Return point set difference of two geometries |  |
| `ST_Dimension()` | ❌ | medium | Dimension of geometry |  |
| `ST_Disjoint()` | ❌ | medium | Whether one geometry is disjoint from another |  |
| `ST_Distance()` | ❌ | medium | The distance of one geometry from another |  |
| `ST_Distance_Sphere()` | ❌ | medium | Minimum distance on earth between two geometries |  |
| `ST_EndPoint()` | ❌ | medium | End Point of LineString |  |
| `ST_Envelope()` | ❌ | medium | Return MBR of geometry |  |
| `ST_Equals()` | ❌ | medium | Whether one geometry is equal to another |  |
| `ST_ExteriorRing()` | ❌ | medium | Return exterior ring of Polygon |  |
| `ST_FrechetDistance()` | ❌ | medium | The discrete Fréchet distance of one geometry from another |  |
| `ST_GeoHash()` | ❌ | medium | Produce a geohash value |  |
| `ST_GeomCollFromText(), ST_GeometryCollectionFromText(), ST_GeomCollFromTxt()` | ❌ | medium | Return geometry collection from WKT |  |
| `ST_GeomCollFromWKB(), ST_GeometryCollectionFromWKB()` | ❌ | medium | Return geometry collection from WKB |  |
| `ST_GeometryN()` | ❌ | medium | Return N-th geometry from geometry collection |  |
| `ST_GeometryType()` | ❌ | medium | Return name of geometry type |  |
| `ST_GeomFromGeoJSON()` | ❌ | medium | Generate geometry from GeoJSON object |  |
| `ST_GeomFromText(), ST_GeometryFromText()` | ❌ | medium | Return geometry from WKT |  |
| `ST_GeomFromWKB(), ST_GeometryFromWKB()` | ❌ | medium | Return geometry from WKB |  |
| `ST_HausdorffDistance()` | ❌ | medium | The discrete Hausdorff distance of one geometry from another |  |
| `ST_InteriorRingN()` | ❌ | medium | Return N-th interior ring of Polygon |  |
| `ST_Intersection()` | ❌ | medium | Return point set intersection of two geometries |  |
| `ST_Intersects()` | ❌ | medium | Whether one geometry intersects another |  |
| `ST_IsClosed()` | ❌ | medium | Whether a geometry is closed and simple |  |
| `ST_IsEmpty()` | ❌ | medium | Whether a geometry is empty |  |
| `ST_IsSimple()` | ❌ | medium | Whether a geometry is simple |  |
| `ST_IsValid()` | ❌ | medium | Whether a geometry is valid |  |
| `ST_LatFromGeoHash()` | ❌ | medium | Return latitude from geohash value |  |
| `ST_Latitude()` | ❌ | medium | Return latitude of Point |  |
| `ST_Length()` | ❌ | medium | Return length of LineString |  |
| `ST_LineFromText(), ST_LineStringFromText()` | ❌ | medium | Construct LineString from WKT |  |
| `ST_LineFromWKB(), ST_LineStringFromWKB()` | ❌ | medium | Construct LineString from WKB |  |
| `ST_LineInterpolatePoint()` | ❌ | medium | The point a given percentage along a LineString |  |
| `ST_LineInterpolatePoints()` | ❌ | medium | The points a given percentage along a LineString |  |
| `ST_LongFromGeoHash()` | ❌ | medium | Return longitude from geohash value |  |
| `ST_Longitude()` | ❌ | medium | Return longitude of Point |  |
| `ST_MakeEnvelope()` | ❌ | medium | Rectangle around two points |  |
| `ST_MLineFromText(), ST_MultiLineStringFromText()` | ❌ | medium | Construct MultiLineString from WKT |  |
| `ST_MLineFromWKB(), ST_MultiLineStringFromWKB()` | ❌ | medium | Construct MultiLineString from WKB |  |
| `ST_MPointFromText(), ST_MultiPointFromText()` | ❌ | medium | Construct MultiPoint from WKT |  |
| `ST_MPointFromWKB(), ST_MultiPointFromWKB()` | ❌ | medium | Construct MultiPoint from WKB |  |
| `ST_MPolyFromText(), ST_MultiPolygonFromText()` | ❌ | medium | Construct MultiPolygon from WKT |  |
| `ST_MPolyFromWKB(), ST_MultiPolygonFromWKB()` | ❌ | medium | Construct MultiPolygon from WKB |  |
| `ST_NumGeometries()` | ❌ | medium | Return number of geometries in geometry collection |  |
| `ST_NumInteriorRing(), ST_NumInteriorRings()` | ❌ | medium | Return number of interior rings in Polygon |  |
| `ST_NumPoints()` | ❌ | medium | Return number of points in LineString |  |
| `ST_Overlaps()` | ❌ | medium | Whether one geometry overlaps another |  |
| `ST_PointAtDistance()` | ❌ | medium | The point a given distance along a LineString |  |
| `ST_PointFromGeoHash()` | ❌ | medium | Convert geohash value to POINT value |  |
| `ST_PointFromText()` | ❌ | medium | Construct Point from WKT |  |
| `ST_PointFromWKB()` | ❌ | medium | Construct Point from WKB |  |
| `ST_PointN()` | ❌ | medium | Return N-th point from LineString |  |
| `ST_PolyFromText(), ST_PolygonFromText()` | ❌ | medium | Construct Polygon from WKT |  |
| `ST_PolyFromWKB(), ST_PolygonFromWKB()` | ❌ | medium | Construct Polygon from WKB |  |
| `ST_Simplify()` | ❌ | medium | Return simplified geometry |  |
| `ST_SRID()` | ❌ | medium | Return spatial reference system ID for geometry |  |
| `ST_StartPoint()` | ❌ | medium | Start Point of LineString |  |
| `ST_SwapXY()` | ❌ | medium | Return argument with X/Y coordinates swapped |  |
| `ST_SymDifference()` | ❌ | medium | Return point set symmetric difference of two geometries |  |
| `ST_Touches()` | ❌ | medium | Whether one geometry touches another |  |
| `ST_Transform()` | ❌ | medium | Transform coordinates of geometry |  |
| `ST_Union()` | ❌ | medium | Return point set union of two geometries |  |
| `ST_Validate()` | ❌ | medium | Return validated geometry |  |
| `ST_Within()` | ❌ | medium | Whether one geometry is within another |  |
| `ST_X()` | ❌ | medium | Return X coordinate of Point |  |
| `ST_Y()` | ❌ | medium | Return Y coordinate of Point |  |
| `STATEMENT_DIGEST()` | ❌ | low | Compute statement digest hash value |  |
| `STATEMENT_DIGEST_TEXT()` | ❌ | low | Compute normalized statement digest |  |
| `STD()` | ❌ | medium | Return the population standard deviation |  |
| `STDDEV()` | ❌ | medium | Return the population standard deviation |  |
| `STDDEV_POP()` | ❌ | medium | Return the population standard deviation |  |
| `STDDEV_SAMP()` | ❌ | medium | Return the sample standard deviation |  |
| `STR_TO_DATE()` | ❌ | high | Convert a string to a date |  |
| `STRCMP()` | ❌ | high | Compare two strings |  |
| `SUBDATE()` | ❌ | top | Synonym for DATE_SUB() when invoked with three arguments |  |
| `SUBSTR()` | ❌ | top | Return the substring as specified |  |
| `SUBSTRING()` | ❌ | top | Return the substring as specified |  |
| `SUBSTRING_INDEX()` | ❌ | high | Return a substring from a string before the specified number of occurrences of the delimiter |  |
| `SUBTIME()` | ❌ | medium | Subtract times |  |
| `SUM()` | 🟡 | top | Return the sum | Implemented for the first single-table aggregate slice, including implicit and grouped aggregates, NULL skipping, empty-input `NULL`, numeric conversion warnings, no-table top-level aggregate calls, metadata, `HAVING`, and `ORDER BY`. `DISTINCT`, window execution, native fixed-point arithmetic, joins, subqueries, and broad type/collation fidelity remain deferred. See [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| `SYSDATE()` | ❌ | medium | Return the time at which the function executes |  |
| `SYSTEM_USER()` | ❌ | high | Synonym for USER() |  |
| `TAN()` | ❌ | medium | Return the tangent of the argument |  |
| `TIME()` | ❌ | medium | Extract the time portion of the expression passed |  |
| `TIME_FORMAT()` | ❌ | high | Format as time |  |
| `TIME_TO_SEC()` | ❌ | high | Return the argument converted to seconds |  |
| `TIMEDIFF()` | ❌ | high | Subtract time |  |
| `TIMESTAMP()` | ❌ | high | With a single argument, this function returns the date or datetime expression; with two arguments, the sum of the arguments |  |
| `TIMESTAMPADD()` | ❌ | high | Add an interval to a datetime expression |  |
| `TIMESTAMPDIFF()` | ❌ | high | Return the difference of two datetime expressions, using the units specified |  |
| `TO_BASE64()` | ❌ | medium | Return the argument converted to a base-64 string |  |
| `TO_DAYS()` | ❌ | high | Return the date argument converted to days |  |
| `TO_SECONDS()` | ❌ | high | Return the date or datetime argument converted to seconds since Year 0 |  |
| `TRIM()` | ❌ | top | Remove leading and trailing spaces |  |
| `TRUNCATE()` | ❌ | medium | Truncate to specified number of decimal places |  |
| `UCASE()` | 🟡 | top | Synonym for UPPER() | Implemented as an `UPPER()` synonym in the Task 24 pure scalar function subset. Non-ASCII case folding remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `UNCOMPRESS()` | ❌ | medium | Uncompress a string compressed |  |
| `UNCOMPRESSED_LENGTH()` | ❌ | medium | Return the length of a string before compression |  |
| `UNHEX()` | ❌ | high | Return a string containing hex representation of a number |  |
| `UNIX_TIMESTAMP()` | ❌ | top | Return a Unix timestamp |  |
| `UpdateXML()` | ❌ | low | Return replaced XML fragment |  |
| `UPPER()` | 🟡 | top | Convert to uppercase | Implemented in the Task 24 pure scalar function subset with ASCII case mapping. Non-ASCII case folding remains deferred. See [scalar built-in functions spec](docs/specs/scalar-built-in-functions/specs.md). |
| `USER()` | ❌ | high | The user name and host name provided by the client |  |
| `UTC_DATE()` | ❌ | high | Return the current UTC date |  |
| `UTC_TIME()` | ❌ | high | Return the current UTC time |  |
| `UTC_TIMESTAMP()` | ❌ | high | Return the current UTC date and time |  |
| `UUID()` | ❌ | high | Return a Universal Unique Identifier (UUID) |  |
| `UUID_SHORT()` | ❌ | high | Return an integer-valued universal identifier |  |
| `UUID_TO_BIN()` | ❌ | high | Convert string UUID to binary |  |
| `VALIDATE_PASSWORD_STRENGTH()` | ❌ | low | Determine strength of password |  |
| `VALUES()` | ❌ | top | Define the values to be used during an INSERT |  |
| `VAR_POP()` | ❌ | medium | Return the population standard variance |  |
| `VAR_SAMP()` | ❌ | medium | Return the sample variance |  |
| `VARIANCE()` | ❌ | medium | Return the population standard variance |  |
| `VERSION()` | ❌ | top | Return a string that indicates the MySQL server version |  |
| `WAIT_FOR_EXECUTED_GTID_SET()` | ❌ | low | Wait until the given GTIDs have executed on the replica. |  |
| `WEEK()` | ❌ | medium | Return the week number |  |
| `WEEKDAY()` | ❌ | medium | Return the weekday index |  |
| `WEEKOFYEAR()` | ❌ | medium | Return the calendar week of the date (1-53) |  |
| `WEIGHT_STRING()` | ❌ | medium | Return the weight string for a string |  |
| `XOR` | 🟡 | medium | Logical XOR | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `YEAR()` | ❌ | high | Return the year |  |
| `YEARWEEK()` | ❌ | medium | Return the year and week |  |
| `\|` | 🟡 | medium | Bitwise OR | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `~` | 🟡 | medium | Bitwise inversion | Implemented for no-table scalar `SELECT` expressions through MyLite expression evaluation, with parser/AST support and runtime tests for Task 16 verified cases. Row expression contexts outside single-table SELECT `WHERE`, full metadata, SQL-mode variants, complete collation behavior, functions, casts, row constructors, and subqueries remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |

## 6. Metadata schemas

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build's availability: present objects need result-shape and privilege tests, while absent optional objects need MySQL-compatible missing-object or unavailable-plugin diagnostics.

### 6.1 INFORMATION_SCHEMA tables

| Table | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS` | ❌ | low | Grantable users or roles for current user or role |  |
| `INFORMATION_SCHEMA.APPLICABLE_ROLES` | ❌ | low | Applicable roles for current user |  |
| `INFORMATION_SCHEMA.CHARACTER_SETS` | ❌ | high | Available character sets |  |
| `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` | ❌ | high | Table and column CHECK constraints |  |
| `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` | ❌ | low | Character set applicable to each collation |  |
| `INFORMATION_SCHEMA.COLLATIONS` | ❌ | high | Collations for each character set |  |
| `INFORMATION_SCHEMA.COLUMN_PRIVILEGES` | ❌ | high | Privileges defined on columns |  |
| `INFORMATION_SCHEMA.COLUMN_STATISTICS` | ❌ | low | Histogram statistics for column values |  |
| `INFORMATION_SCHEMA.COLUMNS` | 🟡 | top | Columns in each table | `SELECT * FROM INFORMATION_SCHEMA.COLUMNS` exposes the MySQL 8.4.9 column shape over the internal column catalog and returns an empty result before table metadata exists; projections, filters, privilege visibility, and self-describing system-view column rows are deferred. See [core metadata catalog spec](docs/specs/core-metadata-catalog/specs.md). |
| `INFORMATION_SCHEMA.COLUMNS_EXTENSIONS` | ❌ | medium | Column attributes for primary and secondary storage engines |  |
| `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` | ❌ | low | Current number of consecutive failed connection attempts per account |  |
| `INFORMATION_SCHEMA.ENABLED_ROLES` | ❌ | low | Roles enabled within current session |  |
| `INFORMATION_SCHEMA.ENGINES` | ❌ | high | Storage engine properties |  |
| `INFORMATION_SCHEMA.EVENTS` | ❌ | medium | Event Manager events |  |
| `INFORMATION_SCHEMA.FILES` | ❌ | low | Files that store tablespace data |  |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` | ❌ | low | Pages in InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` | ❌ | low | LRU ordering of pages in InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS` | ❌ | low | InnoDB buffer pool statistics |  |
| `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES` | ❌ | low | Number of index pages cached per index in InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_CMP` | ❌ | low | Status for operations related to compressed InnoDB tables |  |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX` | ❌ | low | Status for operations related to compressed InnoDB tables and indexes |  |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET` | ❌ | low | Status for operations related to compressed InnoDB tables and indexes |  |
| `INFORMATION_SCHEMA.INNODB_CMP_RESET` | ❌ | low | Status for operations related to compressed InnoDB tables |  |
| `INFORMATION_SCHEMA.INNODB_CMPMEM` | ❌ | low | Status for compressed pages within InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_CMPMEM_RESET` | ❌ | low | Status for compressed pages within InnoDB buffer pool |  |
| `INFORMATION_SCHEMA.INNODB_COLUMNS` | ❌ | medium | Columns in each InnoDB table |  |
| `INFORMATION_SCHEMA.INNODB_DATAFILES` | ❌ | low | Data file path information for InnoDB file-per-table and general tablespaces |  |
| `INFORMATION_SCHEMA.INNODB_FIELDS` | ❌ | medium | Key columns of InnoDB indexes |  |
| `INFORMATION_SCHEMA.INNODB_FOREIGN` | ❌ | medium | InnoDB foreign-key metadata |  |
| `INFORMATION_SCHEMA.INNODB_FOREIGN_COLS` | ❌ | medium | InnoDB foreign-key column status information |  |
| `INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED` | ❌ | low | Snapshot of INNODB_FT_DELETED table |  |
| `INFORMATION_SCHEMA.INNODB_FT_CONFIG` | ❌ | low | Metadata for InnoDB table FULLTEXT index and associated processing |  |
| `INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD` | ❌ | low | Default list of stopwords for InnoDB FULLTEXT indexes |  |
| `INFORMATION_SCHEMA.INNODB_FT_DELETED` | ❌ | low | Rows deleted from InnoDB table FULLTEXT index |  |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE` | ❌ | low | Token information for newly inserted rows in InnoDB FULLTEXT index |  |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE` | ❌ | low | Inverted index information for processing text searches against InnoDB table FULLTEXT index |  |
| `INFORMATION_SCHEMA.INNODB_INDEXES` | ❌ | medium | InnoDB index metadata |  |
| `INFORMATION_SCHEMA.INNODB_METRICS` | ❌ | low | InnoDB performance information |  |
| `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES` | ❌ | low | Session temporary-tablespace metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLES` | ❌ | medium | InnoDB table metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES` | ❌ | medium | InnoDB file-per-table, general, and undo tablespace metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` | ❌ | medium | Brief file-per-table, general, undo, and system tablespace metadata |  |
| `INFORMATION_SCHEMA.INNODB_TABLESTATS` | ❌ | medium | InnoDB table low-level status information |  |
| `INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO` | ❌ | low | Information about active user-created InnoDB temporary tables |  |
| `INFORMATION_SCHEMA.INNODB_TRX` | ❌ | low | Active InnoDB transaction information |  |
| `INFORMATION_SCHEMA.INNODB_VIRTUAL` | ❌ | low | InnoDB virtual generated column metadata |  |
| `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` | ❌ | top | Which key columns have constraints |  |
| `INFORMATION_SCHEMA.KEYWORDS` | ❌ | medium | MySQL keywords |  |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS` | ❌ | low | Firewall in-memory data for account profiles Deprecated in MySQL 8.4. |  |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_WHITELIST` | ❌ | low | Firewall in-memory data for account profile allowlists Deprecated in MySQL 8.4. |  |
| `INFORMATION_SCHEMA.ndb_transid_mysql_connection_map` | ❌ | low | NDB Cluster transaction mapping table when the NDB information-schema plugin is available; unavailable in standard MySQL 8.4 Server. | Conditional NDB Cluster surface. |
| `INFORMATION_SCHEMA.OPTIMIZER_TRACE` | ❌ | medium | Information produced by optimizer trace activity |  |
| `INFORMATION_SCHEMA.PARAMETERS` | ❌ | high | Stored routine parameters and stored function return values |  |
| `INFORMATION_SCHEMA.PARTITIONS` | ❌ | high | Table partition information |  |
| `INFORMATION_SCHEMA.PLUGINS` | ❌ | medium | Plugin information |  |
| `INFORMATION_SCHEMA.PROCESSLIST` | ❌ | high | Information about currently executing threads |  |
| `INFORMATION_SCHEMA.PROFILING` | ❌ | low | Statement profiling information |  |
| `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` | ❌ | high | Foreign key information |  |
| `INFORMATION_SCHEMA.RESOURCE_GROUPS` | ❌ | low | Resource group information |  |
| `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS` | ❌ | low | Column privileges for roles available to or granted by currently enabled roles |  |
| `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS` | ❌ | low | Routine privileges for roles available to or granted by currently enabled roles |  |
| `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS` | ❌ | low | Table privileges for roles available to or granted by currently enabled roles |  |
| `INFORMATION_SCHEMA.ROUTINES` | ❌ | high | Stored routine information |  |
| `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES` | ❌ | high | Privileges defined on schemas |  |
| `INFORMATION_SCHEMA.SCHEMATA` | 🟡 | top | Schema information | `SELECT * FROM INFORMATION_SCHEMA.SCHEMATA` exposes MyLite schema catalog rows, seeded system schemas, schema defaults, and create/drop changes; projections, filters, privileges, and full result metadata are deferred. See [core metadata catalog spec](docs/specs/core-metadata-catalog/specs.md). |
| `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` | ❌ | medium | Schema options |  |
| `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` | ❌ | medium | Columns in each table that store spatial data |  |
| `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` | ❌ | medium | Available spatial reference systems |  |
| `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` | ❌ | medium | Acceptable units for ST_Distance() |  |
| `INFORMATION_SCHEMA.STATISTICS` | 🟡 | top | Table index statistics | `SELECT * FROM INFORMATION_SCHEMA.STATISTICS` exposes the MySQL 8.4.9 column shape over the internal index catalog and returns an empty result before index metadata exists; projections, filters, privileges, and full index metadata are deferred. See [core metadata catalog spec](docs/specs/core-metadata-catalog/specs.md). |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` | ❌ | high | Which tables have constraints |  |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` | ❌ | medium | Table constraint attributes for primary and secondary storage engines |  |
| `INFORMATION_SCHEMA.TABLE_PRIVILEGES` | ❌ | high | Privileges defined on tables |  |
| `INFORMATION_SCHEMA.TABLES` | 🟡 | top | Table information | `SELECT * FROM INFORMATION_SCHEMA.TABLES` exposes the MySQL 8.4.9 column shape, four scoped information-schema system-view rows, and the internal table catalog with no user-object rows before table metadata exists; projections, filters, privileges, and full table metadata are deferred. See [core metadata catalog spec](docs/specs/core-metadata-catalog/specs.md). |
| `INFORMATION_SCHEMA.TABLES_EXTENSIONS` | ❌ | medium | Table attributes for primary and secondary storage engines |  |
| `INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS` | ❌ | low | Tablespace attributes for primary storage engines |  |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATE` | ❌ | low | Thread pool thread group states |  |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS` | ❌ | low | Thread pool thread group statistics |  |
| `INFORMATION_SCHEMA.TP_THREAD_STATE` | ❌ | low | Thread pool thread information |  |
| `INFORMATION_SCHEMA.TRIGGERS` | ❌ | high | Trigger information |  |
| `INFORMATION_SCHEMA.USER_ATTRIBUTES` | ❌ | low | User comments and attributes |  |
| `INFORMATION_SCHEMA.USER_PRIVILEGES` | ❌ | high | Privileges defined globally per user |  |
| `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` | ❌ | low | Stored functions used in views |  |
| `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` | ❌ | low | Tables and views used in views |  |
| `INFORMATION_SCHEMA.VIEWS` | ❌ | high | View information |  |

### 6.2 Performance schema tables

| Table | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `performance_schema.accounts` | ❌ | medium | Connection statistics per client account |  |
| `performance_schema.binary_log_transaction_compression_stats` | ❌ | low | Binary log transaction compression |  |
| `performance_schema.clone_progress` | ❌ | low | Clone operation progress |  |
| `performance_schema.clone_status` | ❌ | low | Clone operation status |  |
| `performance_schema.component_scheduler_tasks` | ❌ | low | Status of scheduled tasks |  |
| `performance_schema.cond_instances` | ❌ | low | Synchronization object instances |  |
| `performance_schema.data_lock_waits` | ❌ | low | Data lock wait relationships |  |
| `performance_schema.data_locks` | ❌ | medium | Data locks held and requested |  |
| `performance_schema.error_log` | ❌ | low | Server error log recent entries |  |
| `performance_schema.events_errors_summary_by_account_by_error` | ❌ | low | Errors per account and error code |  |
| `performance_schema.events_errors_summary_by_host_by_error` | ❌ | low | Errors per host and error code |  |
| `performance_schema.events_errors_summary_by_thread_by_error` | ❌ | low | Errors per thread and error code |  |
| `performance_schema.events_errors_summary_by_user_by_error` | ❌ | low | Errors per user and error code |  |
| `performance_schema.events_errors_summary_global_by_error` | ❌ | low | Errors per error code |  |
| `performance_schema.events_stages_current` | ❌ | low | Current stage events |  |
| `performance_schema.events_stages_history` | ❌ | low | Most recent stage events per thread |  |
| `performance_schema.events_stages_history_long` | ❌ | low | Most recent stage events overall |  |
| `performance_schema.events_stages_summary_by_account_by_event_name` | ❌ | low | Stage events per account and event name |  |
| `performance_schema.events_stages_summary_by_host_by_event_name` | ❌ | low | Stage events per host name and event name |  |
| `performance_schema.events_stages_summary_by_thread_by_event_name` | ❌ | low | Stage waits per thread and event name |  |
| `performance_schema.events_stages_summary_by_user_by_event_name` | ❌ | low | Stage events per user name and event name |  |
| `performance_schema.events_stages_summary_global_by_event_name` | ❌ | low | Stage waits per event name |  |
| `performance_schema.events_statements_current` | ❌ | medium | Current statement events |  |
| `performance_schema.events_statements_histogram_by_digest` | ❌ | medium | Statement histograms per schema and digest value |  |
| `performance_schema.events_statements_histogram_global` | ❌ | medium | Statement histogram summarized globally |  |
| `performance_schema.events_statements_history` | ❌ | medium | Most recent statement events per thread |  |
| `performance_schema.events_statements_history_long` | ❌ | medium | Most recent statement events overall |  |
| `performance_schema.events_statements_summary_by_account_by_event_name` | ❌ | medium | Statement events per account and event name |  |
| `performance_schema.events_statements_summary_by_digest` | ❌ | medium | Statement events per schema and digest value |  |
| `performance_schema.events_statements_summary_by_host_by_event_name` | ❌ | medium | Statement events per host name and event name |  |
| `performance_schema.events_statements_summary_by_program` | ❌ | medium | Statement events per stored program |  |
| `performance_schema.events_statements_summary_by_thread_by_event_name` | ❌ | medium | Statement events per thread and event name |  |
| `performance_schema.events_statements_summary_by_user_by_event_name` | ❌ | medium | Statement events per user name and event name |  |
| `performance_schema.events_statements_summary_global_by_event_name` | ❌ | medium | Statement events per event name |  |
| `performance_schema.events_transactions_current` | ❌ | medium | Current transaction events |  |
| `performance_schema.events_transactions_history` | ❌ | medium | Most recent transaction events per thread |  |
| `performance_schema.events_transactions_history_long` | ❌ | medium | Most recent transaction events overall |  |
| `performance_schema.events_transactions_summary_by_account_by_event_name` | ❌ | medium | Transaction events per account and event name |  |
| `performance_schema.events_transactions_summary_by_host_by_event_name` | ❌ | medium | Transaction events per host name and event name |  |
| `performance_schema.events_transactions_summary_by_thread_by_event_name` | ❌ | medium | Transaction events per thread and event name |  |
| `performance_schema.events_transactions_summary_by_user_by_event_name` | ❌ | medium | Transaction events per user name and event name |  |
| `performance_schema.events_transactions_summary_global_by_event_name` | ❌ | medium | Transaction events per event name |  |
| `performance_schema.events_waits_current` | ❌ | low | Current wait events |  |
| `performance_schema.events_waits_history` | ❌ | low | Most recent wait events per thread |  |
| `performance_schema.events_waits_history_long` | ❌ | low | Most recent wait events overall |  |
| `performance_schema.events_waits_summary_by_account_by_event_name` | ❌ | low | Wait events per account and event name |  |
| `performance_schema.events_waits_summary_by_host_by_event_name` | ❌ | low | Wait events per host name and event name |  |
| `performance_schema.events_waits_summary_by_instance` | ❌ | low | Wait events per instance |  |
| `performance_schema.events_waits_summary_by_thread_by_event_name` | ❌ | low | Wait events per thread and event name |  |
| `performance_schema.events_waits_summary_by_user_by_event_name` | ❌ | low | Wait events per user name and event name |  |
| `performance_schema.events_waits_summary_global_by_event_name` | ❌ | low | Wait events per event name |  |
| `performance_schema.file_instances` | ❌ | low | File instances |  |
| `performance_schema.file_summary_by_event_name` | ❌ | low | File events per event name |  |
| `performance_schema.file_summary_by_instance` | ❌ | low | File events per file instance |  |
| `performance_schema.firewall_group_allowlist` | ❌ | low | Firewall in-memory data for group profile allowlists |  |
| `performance_schema.firewall_groups` | ❌ | low | Firewall in-memory data for group profiles |  |
| `performance_schema.firewall_membership` | ❌ | low | Firewall in-memory data for group profile members |  |
| `performance_schema.global_status` | ❌ | high | Global status variables |  |
| `performance_schema.global_variables` | ❌ | high | Global system variables |  |
| `performance_schema.host_cache` | ❌ | low | Information from internal host cache |  |
| `performance_schema.hosts` | ❌ | medium | Connection statistics per client host name |  |
| `performance_schema.keyring_component_status` | ❌ | low | Status information for installed keyring component |  |
| `performance_schema.keyring_keys` | ❌ | low | Metadata for keyring keys |  |
| `performance_schema.log_status` | ❌ | low | Information about server logs for backup purposes |  |
| `performance_schema.memory_summary_by_account_by_event_name` | ❌ | low | Memory operations per account and event name |  |
| `performance_schema.memory_summary_by_host_by_event_name` | ❌ | low | Memory operations per host and event name |  |
| `performance_schema.memory_summary_by_thread_by_event_name` | ❌ | low | Memory operations per thread and event name |  |
| `performance_schema.memory_summary_by_user_by_event_name` | ❌ | low | Memory operations per user and event name |  |
| `performance_schema.memory_summary_global_by_event_name` | ❌ | low | Memory operations globally per event name |  |
| `performance_schema.metadata_locks` | ❌ | medium | Metadata locks and lock requests |  |
| `performance_schema.mutex_instances` | ❌ | low | Mutex synchronization object instances |  |
| `performance_schema.ndb_sync_excluded_objects` | ❌ | low | NDB objects which cannot be synchronized |  |
| `performance_schema.ndb_sync_pending_objects` | ❌ | low | NDB objects waiting for synchronization |  |
| `performance_schema.objects_summary_global_by_type` | ❌ | low | Object summaries |  |
| `performance_schema.performance_timers` | ❌ | low | Which event timers are available |  |
| `performance_schema.persisted_variables` | ❌ | low | Contents of mysqld-auto.cnf file |  |
| `performance_schema.prepared_statements_instances` | ❌ | high | Prepared statement instances and statistics |  |
| `performance_schema.processlist` | ❌ | high | Process list information |  |
| `performance_schema.replication_applier_configuration` | ❌ | low | Configuration parameters for replication applier on replica |  |
| `performance_schema.replication_applier_filters` | ❌ | low | Channel-specific replication filters on current replica |  |
| `performance_schema.replication_applier_global_filters` | ❌ | low | Global replication filters on current replica |  |
| `performance_schema.replication_applier_status` | ❌ | low | Current status of replication applier on replica |  |
| `performance_schema.replication_applier_status_by_coordinator` | ❌ | low | SQL or coordinator thread applier status |  |
| `performance_schema.replication_applier_status_by_worker` | ❌ | low | Worker thread applier status |  |
| `performance_schema.replication_asynchronous_connection_failover` | ❌ | low | Source lists for asynchronous connection failover mechanism |  |
| `performance_schema.replication_asynchronous_connection_failover_managed` | ❌ | low | Managed source lists for asynchronous connection failover mechanism |  |
| `performance_schema.replication_connection_configuration` | ❌ | low | Configuration parameters for connecting to source |  |
| `performance_schema.replication_connection_status` | ❌ | low | Current status of connection to source |  |
| `performance_schema.replication_group_communication_information` | ❌ | low | Replication group configuration options |  |
| `performance_schema.replication_group_configuration_version` | ❌ | low | Version of the member actions configuration for replication group members |  |
| `performance_schema.replication_group_member_actions` | ❌ | low | Member actions that are included in the member actions configuration for replication group members |  |
| `performance_schema.replication_group_member_stats` | ❌ | low | Replication group member statistics |  |
| `performance_schema.replication_group_members` | ❌ | low | Replication group member network and status |  |
| `performance_schema.rwlock_instances` | ❌ | low | Lock synchronization object instances |  |
| `performance_schema.session_account_connect_attrs` | ❌ | medium | Connection attributes per for current session |  |
| `performance_schema.session_connect_attrs` | ❌ | medium | Connection attributes for all sessions |  |
| `performance_schema.session_status` | ❌ | high | Status variables for current session |  |
| `performance_schema.session_variables` | ❌ | high | System variables for current session |  |
| `performance_schema.setup_actors` | ❌ | low | How to initialize monitoring for new foreground threads |  |
| `performance_schema.setup_consumers` | ❌ | low | Consumers for which event information can be stored |  |
| `performance_schema.setup_instruments` | ❌ | low | Classes of instrumented objects for which events can be collected |  |
| `performance_schema.setup_objects` | ❌ | low | Which objects should be monitored |  |
| `performance_schema.setup_threads` | ❌ | medium | Instrumented thread names and attributes |  |
| `performance_schema.socket_instances` | ❌ | low | Active connection instances |  |
| `performance_schema.socket_summary_by_event_name` | ❌ | low | Socket waits and I/O per event name |  |
| `performance_schema.socket_summary_by_instance` | ❌ | low | Socket waits and I/O per instance |  |
| `performance_schema.status_by_account` | ❌ | medium | Session status variables per account |  |
| `performance_schema.status_by_host` | ❌ | medium | Session status variables per host name |  |
| `performance_schema.status_by_thread` | ❌ | medium | Session status variables per session |  |
| `performance_schema.status_by_user` | ❌ | medium | Session status variables per user name |  |
| `performance_schema.table_handles` | ❌ | low | Table locks and lock requests |  |
| `performance_schema.table_io_waits_summary_by_index_usage` | ❌ | low | Table I/O waits per index |  |
| `performance_schema.table_io_waits_summary_by_table` | ❌ | medium | Table I/O waits per table |  |
| `performance_schema.table_lock_waits_summary_by_table` | ❌ | low | Table lock waits per table |  |
| `performance_schema.threads` | ❌ | medium | Information about server threads |  |
| `performance_schema.tls_channel_status` | ❌ | low | TLS status for each connection interface |  |
| `performance_schema.tp_thread_group_state` | ❌ | low | Thread pool thread group states |  |
| `performance_schema.tp_thread_group_stats` | ❌ | low | Thread pool thread group statistics |  |
| `performance_schema.tp_thread_state` | ❌ | low | Thread pool thread information |  |
| `performance_schema.user_defined_functions` | ❌ | low | Registered loadable functions |  |
| `performance_schema.user_variables_by_thread` | ❌ | high | User-defined variables per thread |  |
| `performance_schema.users` | ❌ | medium | Connection statistics per client user name |  |
| `performance_schema.variables_by_thread` | ❌ | high | Session system variables per session |  |
| `performance_schema.variables_info` | ❌ | high | How system variables were most recently set |  |
| `performance_schema.tp_connections` | ❌ | low | Thread pool connection state and queue information. |  |

### 6.3 sys schema objects

| Object | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `sys.sys_config` | ❌ | low | Expose MySQL-compatible sys schema table behavior, result shape, and diagnostics. |  |
| `sys.sys_config_insert_set_user` | ❌ | low | Expose MySQL-compatible sys schema trigger behavior, result shape, and diagnostics. |  |
| `sys.sys_config_update_set_user` | ❌ | low | Expose MySQL-compatible sys schema trigger behavior, result shape, and diagnostics. |  |
| `sys.host_summary` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_file_io` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_file_io` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_file_io_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_file_io_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_stages` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_stages` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_statement_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_statement_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.host_summary_by_statement_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$host_summary_by_statement_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.innodb_buffer_stats_by_schema` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$innodb_buffer_stats_by_schema` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.innodb_buffer_stats_by_table` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$innodb_buffer_stats_by_table` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.innodb_lock_waits` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$innodb_lock_waits` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_by_thread_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_by_thread_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_file_by_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_file_by_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_file_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_file_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_wait_by_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_wait_by_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.io_global_by_wait_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$io_global_by_wait_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.latest_file_io` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$latest_file_io` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_by_host_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_by_host_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_by_thread_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_by_thread_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_by_user_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_by_user_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_global_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_global_by_current_bytes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.memory_global_total` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$memory_global_total` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.metrics` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.processlist` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$processlist` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$ps_digest_95th_percentile_by_avg_us` | ❌ | low | Expose MySQL-compatible sys schema helper view behavior, result shape, and diagnostics. | Helper view for statement runtime percentile reporting. |
| `sys.x$ps_digest_avg_latency_distribution` | ❌ | low | Expose MySQL-compatible sys schema helper view behavior, result shape, and diagnostics. | Helper view for statement runtime percentile reporting. |
| `sys.x$ps_schema_table_statistics_io` | ❌ | medium | Expose MySQL-compatible sys schema helper view behavior, result shape, and diagnostics. | Helper view for schema table statistics. |
| `sys.ps_check_lost_instrumentation` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_auto_increment_columns` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_index_statistics` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_index_statistics` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_object_overview` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_redundant_indexes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_flattened_keys` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_table_lock_waits` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_table_lock_waits` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_table_statistics` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_table_statistics` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_table_statistics_with_buffer` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_table_statistics_with_buffer` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_tables_with_full_table_scans` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$schema_tables_with_full_table_scans` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.schema_unused_indexes` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.session` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$session` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.session_ssl_status` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statement_analysis` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statement_analysis` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_errors_or_warnings` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_errors_or_warnings` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_full_table_scans` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_full_table_scans` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_runtimes_in_95th_percentile` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_runtimes_in_95th_percentile` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_sorting` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_sorting` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.statements_with_temp_tables` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$statements_with_temp_tables` | ❌ | medium | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_file_io` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_file_io` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_file_io_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_file_io_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_stages` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_stages` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_statement_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_statement_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.user_summary_by_statement_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$user_summary_by_statement_type` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.version` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.wait_classes_global_by_avg_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$wait_classes_global_by_avg_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.wait_classes_global_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$wait_classes_global_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.waits_by_host_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$waits_by_host_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.waits_by_user_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$waits_by_user_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.waits_global_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.x$waits_global_by_latency` | ❌ | low | Expose MySQL-compatible sys schema view behavior, result shape, and diagnostics. |  |
| `sys.create_synonym_db()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.diagnostics()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.execute_prepared_stmt()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_background_threads()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_consumer()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_instrument()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_disable_thread()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_background_threads()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_consumer()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_instrument()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_enable_thread()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_reload_saved()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_reset_to_default()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_save()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_disabled()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_disabled_consumers()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_disabled_instruments()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_enabled()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_enabled_consumers()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_setup_show_enabled_instruments()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_statement_avg_latency_histogram()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_trace_statement_digest()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_trace_thread()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.ps_truncate_all_tables()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.statement_performance_analyzer()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.table_exists()` | ❌ | low | Expose MySQL-compatible sys schema procedure behavior, result shape, and diagnostics. |  |
| `sys.extract_schema_from_file_name()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.extract_table_from_file_name()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_bytes()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_path()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_statement()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.format_time()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.list_add()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.list_drop()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_account_enabled()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_consumer_enabled()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_instrument_default_enabled()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_instrument_default_timed()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_is_thread_instrumented()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_account()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_id()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_stack()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.ps_thread_trx_info()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.quote_identifier()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.sys_get_config()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.version_major()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.version_minor()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |
| `sys.version_patch()` | ❌ | low | Expose MySQL-compatible sys schema function behavior, result shape, and diagnostics. |  |

### 6.4 mysql system schema and data dictionary

| Table | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `mysql.catalogs` | ❌ | low | Invisible data dictionary table for catalog metadata. |  |
| `mysql.character_sets` | ❌ | top | Invisible data dictionary table for character set metadata. |  |
| `mysql.check_constraints` | ❌ | high | Invisible data dictionary table for CHECK constraint metadata. |  |
| `mysql.collations` | ❌ | top | Invisible data dictionary table for collation metadata. |  |
| `mysql.column_statistics` | ❌ | low | Invisible data dictionary table for histogram statistics. |  |
| `mysql.column_type_elements` | ❌ | top | Invisible data dictionary table for ENUM/SET and other column type elements. |  |
| `mysql.columns` | ❌ | top | Invisible data dictionary table for table column metadata. |  |
| `mysql.dd_properties` | ❌ | low | Invisible data dictionary table for dictionary version and upgrade metadata. |  |
| `mysql.events` | ❌ | medium | Invisible data dictionary table for Event Scheduler metadata. |  |
| `mysql.foreign_keys` | ❌ | high | Invisible data dictionary table for foreign key metadata. |  |
| `mysql.foreign_key_column_usage` | ❌ | high | Invisible data dictionary table for foreign key column mappings. |  |
| `mysql.index_column_usage` | ❌ | low | Invisible data dictionary table for index column usage. |  |
| `mysql.index_partitions` | ❌ | low | Invisible data dictionary table for index partition metadata. |  |
| `mysql.index_stats` | ❌ | medium | Invisible data dictionary table for dynamic index statistics. |  |
| `mysql.indexes` | ❌ | top | Invisible data dictionary table for index metadata. |  |
| `mysql.innodb_ddl_log` | ❌ | medium | Invisible data dictionary table for crash-safe DDL logs. |  |
| `mysql.parameter_type_elements` | ❌ | low | Invisible data dictionary table for routine parameter and return type elements. |  |
| `mysql.parameters` | ❌ | high | Invisible data dictionary table for stored routine parameters and function return values. |  |
| `mysql.resource_groups` | ❌ | low | Invisible data dictionary table for resource group metadata. |  |
| `mysql.routines` | ❌ | high | Invisible data dictionary table for stored procedure and function metadata. |  |
| `mysql.schemata` | ❌ | top | Invisible data dictionary table for schema metadata. |  |
| `mysql.st_spatial_reference_systems` | ❌ | low | Invisible data dictionary table for spatial reference systems. |  |
| `mysql.table_partition_values` | ❌ | low | Invisible data dictionary table for partition values. |  |
| `mysql.table_partitions` | ❌ | low | Invisible data dictionary table for table partition metadata. |  |
| `mysql.table_stats` | ❌ | medium | Invisible data dictionary table for dynamic table statistics. |  |
| `mysql.tables` | ❌ | top | Invisible data dictionary table for table metadata. |  |
| `mysql.tablespace_files` | ❌ | low | Invisible data dictionary table for tablespace files. |  |
| `mysql.tablespaces` | ❌ | low | Invisible data dictionary table for active tablespaces. |  |
| `mysql.triggers` | ❌ | high | Invisible data dictionary table for trigger metadata. |  |
| `mysql.view_routine_usage` | ❌ | low | Invisible data dictionary table for view-to-routine dependencies. |  |
| `mysql.view_table_usage` | ❌ | low | Invisible data dictionary table for view-to-table dependencies. |  |
| `mysql.user` | ❌ | high | Grant table for accounts, global privileges, authentication, and nonprivilege account attributes. |  |
| `mysql.global_grants` | ❌ | low | Grant table for dynamic global privilege assignments. |  |
| `mysql.db` | ❌ | high | Grant table for database-level privileges. |  |
| `mysql.tables_priv` | ❌ | high | Grant table for table-level privileges. |  |
| `mysql.columns_priv` | ❌ | high | Grant table for column-level privileges. |  |
| `mysql.procs_priv` | ❌ | medium | Grant table for routine privileges. |  |
| `mysql.proxies_priv` | ❌ | low | Grant table for proxy-user privileges. |  |
| `mysql.default_roles` | ❌ | low | Grant table for default role activation. |  |
| `mysql.role_edges` | ❌ | low | Grant table for role graph edges. |  |
| `mysql.password_history` | ❌ | low | Grant table for password history. |  |
| `mysql.component` | ❌ | medium | Registry for server components installed with INSTALL COMPONENT. |  |
| `mysql.func` | ❌ | medium | Registry for loadable functions installed with CREATE FUNCTION. |  |
| `mysql.plugin` | ❌ | medium | Registry for server-side plugins installed with INSTALL PLUGIN. |  |
| `mysql.general_log` | ❌ | medium | CSV log table for the general query log. |  |
| `mysql.slow_log` | ❌ | medium | CSV log table for the slow query log. |  |
| `mysql.help_category` | ❌ | low | Server-side HELP category table. |  |
| `mysql.help_keyword` | ❌ | low | Server-side HELP keyword table. |  |
| `mysql.help_relation` | ❌ | low | Server-side HELP relation table. |  |
| `mysql.help_topic` | ❌ | low | Server-side HELP topic table. |  |
| `mysql.time_zone` | ❌ | high | Time zone ID and leap-second usage table. |  |
| `mysql.time_zone_leap_second` | ❌ | high | Leap-second transition table. |  |
| `mysql.time_zone_name` | ❌ | high | Time zone name mapping table. |  |
| `mysql.time_zone_transition` | ❌ | high | Time zone transition table. |  |
| `mysql.time_zone_transition_type` | ❌ | high | Time zone transition type table. |  |
| `mysql.gtid_executed` | ❌ | low | Replication table storing GTID values. |  |
| `mysql.ndb_binlog_index` | ❌ | low | NDB Cluster replication binary log information table. |  |
| `mysql.slave_master_info` | ❌ | low | Replication metadata repository table for source connection metadata. |  |
| `mysql.slave_relay_log_info` | ❌ | low | Replication metadata repository table for relay log metadata. |  |
| `mysql.slave_worker_info` | ❌ | low | Replication metadata repository table for worker metadata. |  |
| `mysql.innodb_index_stats` | ❌ | medium | Optimizer table for InnoDB persistent index statistics. |  |
| `mysql.innodb_table_stats` | ❌ | medium | Optimizer table for InnoDB persistent table statistics. |  |
| `mysql.server_cost` | ❌ | medium | Optimizer cost model table for general server operation costs. |  |
| `mysql.engine_cost` | ❌ | medium | Optimizer cost model table for storage-engine operation costs. |  |
| `mysql.audit_log_filter` | ❌ | low | Enterprise Audit table for persistent audit filter definitions. |  |
| `mysql.audit_log_user` | ❌ | low | Enterprise Audit table for persistent audit user mappings. |  |
| `mysql.firewall_group_allowlist` | ❌ | low | Enterprise Firewall table for group profile allowlists. |  |
| `mysql.firewall_groups` | ❌ | low | Enterprise Firewall table for group profiles. |  |
| `mysql.firewall_membership` | ❌ | low | Enterprise Firewall table for group profile memberships. |  |
| `mysql.firewall_users` | ❌ | low | Enterprise Firewall table for account profiles. |  |
| `mysql.firewall_whitelist` | ❌ | low | Deprecated Enterprise Firewall allowlist table. |  |
| `mysql.servers` | ❌ | low | FEDERATED storage engine server definition table. |  |
| `mysql.innodb_dynamic_metadata` | ❌ | medium | InnoDB table for fast-changing table metadata such as auto-increment counters. |  |

## 7. Runtime configuration, modes, and variables

### 7.1 Session and runtime state

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| Default schema | 🟡 | top | DATABASE()/SCHEMA(), USE, schema-qualified names, and single-file mapping. | `USE` stores handle-owned default schema state and dropping the selected schema clears it; `DATABASE()`/`SCHEMA()` expressions and schema-qualified object execution are deferred. See [schema lifecycle spec](docs/specs/schema-lifecycle/specs.md). |
| Connection character set state | 🟡 | top | character_set_client, character_set_connection, character_set_results, collation_connection, and SET NAMES behavior. | Handle-owned state is implemented for `SET NAMES` and `SET CHARACTER SET` over the initial registry; general `SHOW VARIABLES`, `SELECT @@...`, protocol metadata, and direct system-variable assignment are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| Time zone state | ❌ | top | global/session time_zone, system_time_zone, temporal functions, TIMESTAMP conversion, and named time zones. |  |
| Autocommit state | 🟡 | top | autocommit behavior, implicit transactions, and transaction boundary metadata. | Default autocommit-on behavior is implemented for explicit Task 21 transaction statements: DML outside an explicit transaction commits statement-by-statement, DML inside an explicit transaction participates in the active transaction with statement-level savepoint rollback, and `AND NO CHAIN` resumes autocommit behavior. Direct `SET autocommit` and variable exposure remain deferred. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| Last insert id | 🟡 | top | LAST_INSERT_ID(), OK packet insert id, explicit LAST_INSERT_ID(expr), and multi-row behavior. | `INSERT ... VALUES` and `INSERT ... SET` track generated `AUTO_INCREMENT` values and expose the session value through `mylite_last_insert_id()`. `INSERT ... VALUES` records the first generated value from an accepted row, including rows later rolled back by a statement failure. `INSERT ... SET` records the generated value from its successful single row. Generated values allocated for rows that fail before insertion consume the sequence without replacing the previous session value. SQL `LAST_INSERT_ID()`/`LAST_INSERT_ID(expr)`, protocol OK packet insert id, and non-insert interactions remain deferred. See [INSERT ... VALUES spec](docs/specs/insert-values/specs.md) and [INSERT ... SET spec](docs/specs/insert-set/specs.md). |
| Affected rows | 🟡 | top | CLIENT_FOUND_ROWS, changed rows vs matched rows, DDL/DML OK packets, and warning counts. | Statement affected rows are exposed through `mylite_affected_rows()` for SQLite-backed statements and custom `INSERT ... VALUES`/`INSERT ... SET` statements. MySQL protocol OK packets, `CLIENT_FOUND_ROWS`, matched-vs-changed update semantics, warning counts, and SQL `ROW_COUNT()` remain deferred. See [INSERT ... VALUES spec](docs/specs/insert-values/specs.md) and [INSERT ... SET spec](docs/specs/insert-set/specs.md). |
| Warnings and diagnostics | 🟡 | high | SHOW WARNINGS, SHOW ERRORS, GET DIAGNOSTICS, warning count, sql_notes, and truncation warnings. | Public warning-count/code/message accessors expose Task 16 scalar expression warnings and Task 17 single-table `WHERE` predicate warnings/errors for division by zero, numeric truncation, invalid `ESCAPE`, and unknown predicate columns. SQL `SHOW WARNINGS`, `SHOW ERRORS`, `GET DIAGNOSTICS`, sql_notes, protocol warning counts, and broader statement warning integration remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md) and [WHERE clause spec](docs/specs/where-clause/specs.md). |
| Prepared statement registry | ❌ | top | Per-connection prepared statement namespace and deallocation behavior. |  |
| Temporary table namespace | ❌ | high | Per-session temporary table metadata and shadowing. |  |
| Role and privilege state | ❌ | high | CURRENT_ROLE(), CURRENT_USER(), DEFINER/INVOKER, and privilege-check-visible metadata. |  |
| Locks | ❌ | high | Named locks, table locks, metadata locks, backup locks, and lock diagnostics. |  |

### 7.2 SQL modes

| SQL mode | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `ALLOW_INVALID_DATES` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ANSI` | ❌ | medium | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ANSI_QUOTES` | ❌ | top | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ERROR_FOR_DIVISION_BY_ZERO` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. | Task 16 specifies verified default-mode expression warnings for `/`, `DIV`, `%`, and `MOD`; complete SQL-mode interactions remain deferred. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `HIGH_NOT_PRECEDENCE` | ❌ | medium | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. | Task 16 specifies default `!` and `NOT` precedence and intentionally defers this mode-sensitive parser behavior. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `IGNORE_SPACE` | ❌ | medium | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_AUTO_VALUE_ON_ZERO` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_BACKSLASH_ESCAPES` | ❌ | top | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_DIR_IN_CREATE` | ❌ | low | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_ENGINE_SUBSTITUTION` | ❌ | top | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_UNSIGNED_SUBTRACTION` | ❌ | medium | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_ZERO_DATE` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `NO_ZERO_IN_DATE` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `ONLY_FULL_GROUP_BY` | ❌ | top | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `PAD_CHAR_TO_FULL_LENGTH` | ❌ | medium | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `PIPES_AS_CONCAT` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. | Task 16 specifies default `||` logical-OR behavior and intentionally defers mode-sensitive string concatenation. See [expression operator foundation spec](docs/specs/expression-operator-foundation/specs.md). |
| `REAL_AS_FLOAT` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `STRICT_ALL_TABLES` | ❌ | medium | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `STRICT_TRANS_TABLES` | ❌ | top | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `TIME_TRUNCATE_FRACTIONAL` | ❌ | low | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |
| `TRADITIONAL` | ❌ | high | Implement parser, expression, DDL, DML, conversion, warning, and error behavior affected by this mode. |  |

### 7.3 Server system variables

The exact value, scope, mutability, privilege requirement, persisted-variable behavior, optional plugin/build availability, and `SHOW VARIABLES`/`performance_schema` exposure must be verified per variable.

| Variable | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `activate_all_roles_on_login` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_address` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_port` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_ca` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_capath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_cert` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_cipher` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_crl` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_crlpath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_ssl_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_tls_ciphersuites` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `admin_tls_version` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_buffer_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_compression` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_connection_policy` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_current_session` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_database` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_disable` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_encryption` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_exclude_accounts` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_file` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_filter_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_flush` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_flush_interval_seconds` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_format` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_format_unix_timestamp` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_include_accounts` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_password_history_keep_days` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_policy` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_prune_seconds` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_read_buffer_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_rotate_on_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_statement_policy` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `audit_log_strategy` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_kerberos_service_key_tab` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_kerberos_service_principal` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_auth_method_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_bind_base_dn` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_bind_root_dn` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_bind_root_pwd` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_ca_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_connect_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_group_search_attr` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_group_search_filter` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_init_pool_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_log_status` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_max_pool_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_referral` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_response_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_server_host` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_server_port` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_tls` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_sasl_user_search_attr` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_auth_method_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_bind_base_dn` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_bind_root_dn` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_bind_root_pwd` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_ca_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_connect_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_group_search_attr` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_group_search_filter` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_init_pool_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_log_status` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_max_pool_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_referral` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_response_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_server_host` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_server_port` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_tls` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_ldap_simple_user_search_attr` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_policy` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_webauthn_rp_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_windows_log_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `authentication_windows_use_principal_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `auto_generate_certs` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `auto_increment_increment` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `auto_increment_offset` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `autocommit` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Task 21 start spec documents default `ON` behavior and transaction-boundary interactions; direct variable exposure and `SET autocommit` remain unimplemented. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `automatic_sp_privileges` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `back_log` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `basedir` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `big_tables` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `bind_address` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_checksum` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_direct_non_transactional_updates` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_encryption` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_error_action` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_expire_logs_auto_purge` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_expire_logs_seconds` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_format` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_group_commit_sync_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_group_commit_sync_no_delay_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_gtid_simple_recovery` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_max_flush_queue_time` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_order_commits` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_rotate_encryption_master_key_at_startup` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_event_max_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_image` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_metadata` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_row_value_options` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_rows_query_log_events` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_stmt_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_transaction_compression` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_transaction_compression_level_zstd` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `binlog_transaction_dependency_history_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `block_encryption_mode` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `build_id` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `bulk_insert_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_auto_generate_rsa_keys` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_digest_rounds` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_private_key_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `caching_sha2_password_public_key_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_client` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Stored as handle-owned session state and updated by `SET NAMES`/`SET CHARACTER SET`; SQL variable exposure and direct assignment are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `character_set_connection` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Stored as handle-owned session state and updated by `SET NAMES`/`SET CHARACTER SET`; SQL variable exposure and direct assignment are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `character_set_database` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Reflected through schema catalog defaults for selected-schema behavior in `SET CHARACTER SET`; SQL variable exposure is deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `character_set_filesystem` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_set_results` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Stored as handle-owned session state and updated by `SET NAMES`/`SET CHARACTER SET`; SQL variable exposure and direct assignment are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `character_set_server` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Fixed internally as `utf8mb4` for default connection and schema behavior; SQL variable exposure is deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `character_set_system` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `character_sets_dir` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `check_proxy_users` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_autotune_concurrency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_block_ddl` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_buffer_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ddl_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_delay_after_data_drop` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_donor_timeout_after_network_failure` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_enable_compression` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_max_concurrency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_max_data_bandwidth` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_max_network_bandwidth` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ssl_ca` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ssl_cert` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_ssl_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `clone_valid_donor_list` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `collation_connection` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Stored as handle-owned session state and updated by `SET NAMES`/`SET CHARACTER SET`; SQL variable exposure and direct assignment are deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `collation_database` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Reflected through schema catalog defaults for selected-schema behavior in `SET CHARACTER SET`; SQL variable exposure is deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `collation_server` | 🟡 | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Fixed internally as `utf8mb4_0900_ai_ci` for default connection and schema behavior; SQL variable exposure is deferred. See [character set/collation foundation spec](docs/specs/character-set-collation-foundation/specs.md). |
| `completion_type` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Task 21 start spec documents how `completion_type` affects bare `COMMIT` and `ROLLBACK`; direct variable exposure and `SET` behavior remain unimplemented. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `component_masking.dictionaries_flush_interval_seconds` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `component_masking.masking_database` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `component_scheduler.enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `concurrent_insert` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connect_timeout` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_control_failed_connections_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_control_max_connection_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_control_min_connection_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_memory_chunk_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `connection_memory_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `core_file` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `create_admin_listener_thread` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `cte_max_recursion_depth` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `datadir` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `debug_sync` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_collation_for_utf8mb4` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_password_lifetime` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_storage_engine` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_table_encryption` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_tmp_storage_engine` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `default_week_format` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delay_key_write` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delayed_insert_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delayed_insert_timeout` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `delayed_queue_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `disabled_storage_engines` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `disconnect_on_expired_password` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `div_precision_increment` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `dragnet.log_error_filter_rules` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `end_markers_in_json` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `enforce_gtid_consistency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `enterprise_encryption.maximum_rsa_key_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `enterprise_encryption.rsa_support_legacy_padding` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `eq_range_index_dive_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `error_count` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `event_scheduler` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `explain_format` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `explain_json_format_version` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `explicit_defaults_for_timestamp` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `external_user` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `flush` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `flush_time` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `foreign_key_checks` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_boolean_syntax` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_max_word_len` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_min_word_len` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_query_expansion_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ft_stopword_file` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `general_log` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `general_log_file` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `generated_random_password_length` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `global_connection_memory_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `global_connection_memory_tracking` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_concat_max_len` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_advertise_recovery_endpoints` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_allow_local_lower_version_join` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_auto_increment_increment` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_autorejoin_tries` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_bootstrap_group` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_clone_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_communication_debug_options` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_communication_max_message_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_communication_stack` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_components_stop_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_compression_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_consistency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_enforce_update_everywhere_checks` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_exit_state_action` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_applier_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_certifier_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_hold_percent` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_max_quota` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_member_quota_percent` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_min_quota` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_min_recovery_quota` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_period` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_flow_control_release_percent` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_force_members` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_group_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_group_seeds` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_gtid_assignment_block_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_ip_allowlist` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_local_address` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_member_expel_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_member_weight` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_message_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_paxos_single_leader` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_poll_spin_loops` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_preemptive_garbage_collection` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_preemptive_garbage_collection_rows_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_compression_algorithms` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_get_public_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_public_key_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_reconnect_interval` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_retry_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_ca` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_capath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_cert` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_cipher` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_crl` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_crlpath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_ssl_verify_server_cert` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_tls_ciphersuites` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_tls_version` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_use_ssl` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_recovery_zstd_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_single_primary_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_ssl_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_start_on_boot` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_tls_source` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_transaction_size_limit` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_unreachable_majority_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `group_replication_view_change_uuid` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_executed` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_executed_compression_period` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_next` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_owned` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `gtid_purged` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_compress` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_dynamic_loading` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_geometry` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_profiling` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_query_cache` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_rtree_keys` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_statement_timeout` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `have_symlink` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `histogram_generation_max_mem_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `host_cache_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `hostname` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `identity` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `immediate_server_version` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `information_schema_stats_expiry` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_connect` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_file` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_replica` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `init_slave` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_flushing` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_flushing_lwm` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_hash_index` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_hash_index_parts` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_adaptive_max_sleep_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_autoextend_increment` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_autoinc_lock_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_background_drop_list_empty` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_chunk_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_dump_at_shutdown` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_dump_now` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_dump_pct` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_filename` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_in_core_file` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_load_abort` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_load_at_startup` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_load_now` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_buffer_pool_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_change_buffer_max_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_change_buffering` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_change_buffering_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_checkpoint_disabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_checksum_algorithm` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_cmp_per_index_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_commit_concurrency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compress_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compression_failure_threshold_pct` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_compression_pad_pct_max` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_concurrency_tickets` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_data_file_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_data_home_dir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ddl_buffer_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ddl_log_crash_reset_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ddl_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_deadlock_detect` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_dedicated_server` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_default_row_format` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_directories` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_disable_sort_file_cache` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_batch_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_dir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_files` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_doublewrite_pages` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_extend_and_initialize` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fast_shutdown` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fil_make_page_dirty_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_file_per_table` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fill_factor` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_log_at_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_log_at_trx_commit` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_method` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_neighbors` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flush_sync` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_flushing_avg_loops` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_force_load_corrupted` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_force_recovery` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_fsync_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_aux_table` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_enable_diag_print` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_enable_stopword` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_max_token_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_min_token_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_num_word_optimize` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_result_cache_limit` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_server_stopword_table` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_sort_pll_degree` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_total_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_ft_user_stopword_table` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_idle_flush_pct` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_io_capacity` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_io_capacity_max` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_limit_optimistic_insert_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_lock_wait_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_buffer_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_checkpoint_fuzzy_now` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_checkpoint_now` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_checksums` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_compressed_pages` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_file_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_files_in_group` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_group_home_dir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_spin_cpu_abs_lwm` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_spin_cpu_pct_hwm` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_wait_for_flush_spin_hwm` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_write_ahead_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_log_writer_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_lru_scan_depth` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_dirty_pages_pct` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_dirty_pages_pct_lwm` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_purge_lag` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_purge_lag_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_max_undo_log_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_merge_threshold_set_all_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_disable` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_enable` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_reset` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_monitor_reset_all` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_numa_interleave` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_old_blocks_pct` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_old_blocks_time` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_online_alter_log_max_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_open_files` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_optimize_fulltext_only` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_page_cleaners` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_page_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_parallel_read_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_print_all_deadlocks` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_print_ddl_logs` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_purge_batch_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_purge_rseg_truncate_frequency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_purge_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_random_read_ahead` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_read_ahead_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_read_io_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_read_only` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_redo_log_archive_dirs` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_redo_log_capacity` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_redo_log_encrypt` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_replication_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_rollback_on_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_rollback_segments` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_saved_page_number_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_segment_reserve_factor` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sort_buffer_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_spin_wait_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_spin_wait_pause_multiplier` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_auto_recalc` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_include_delete_marked` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_method` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_on_metadata` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_persistent` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_persistent_sample_pages` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_stats_transient_sample_pages` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_status_output` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_status_output_locks` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_strict_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sync_array_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sync_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_sync_spin_loops` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_table_locks` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_temp_data_file_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_temp_tablespaces_dir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_thread_concurrency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_thread_sleep_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_tmpdir` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_trx_purge_view_update_only_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_trx_rseg_n_slots_debug` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_directory` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_log_encrypt` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_log_truncate` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_undo_tablespaces` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_use_fdatasync` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_use_native_aio` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_validate_tablespace_paths` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_version` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `innodb_write_io_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `insert_id` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `interactive_timeout` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `internal_tmp_mem_storage_engine` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `join_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keep_files_on_create` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_cache_age_threshold` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_cache_block_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `key_cache_division_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_cmk_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_conf_file` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_data_file` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_aws_region` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_auth_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_ca_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_caching` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_auth_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_ca_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_caching` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_role_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_server_url` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_commit_store_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_role_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_secret_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_server_url` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_hashicorp_store_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_okv_conf_dir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `keyring_operations` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `large_files_support` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `large_page_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `large_pages` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `last_insert_id` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lc_messages` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lc_messages_dir` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lc_time_names` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `license` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `local_infile` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_loop` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_missing_arc` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_missing_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_debug_missing_unlock` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_dependencies` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_extra_dependencies` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_output_directory` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_print_txt` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_loop` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_missing_arc` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_missing_key` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_order_trace_missing_unlock` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lock_wait_timeout` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `locked_in_memory` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin_basename` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin_index` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_bin_trust_function_creators` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error_services` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error_suppression_list` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_error_verbosity` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_output` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_queries_not_using_indexes` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_raw` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_replica_updates` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slave_updates` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_admin_statements` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_extra` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_replica_statements` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_slow_slave_statements` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_statements_unsafe_for_binlog` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_throttle_queries_not_using_indexes` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `log_timestamps` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `long_query_time` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `low_priority_updates` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lower_case_file_system` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `lower_case_table_names` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mandatory_roles` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `master_verify_checksum` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_allowed_packet` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_binlog_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_binlog_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_binlog_stmt_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_connect_errors` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_connections` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_delayed_threads` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_digest_length` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_error_count` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_execution_time` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_heap_table_size` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_insert_delayed_threads` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_join_size` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_length_for_sort_data` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_points_in_geometry` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_prepared_stmt_count` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_relay_log_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_seeks_for_key` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_sort_length` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_sp_recursion_depth` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_user_connections` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `max_write_lock_count` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mecab_rc_file` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `min_examined_row_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_data_pointer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_max_sort_file_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_mmap_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_recover_options` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_sort_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_stats_method` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `myisam_use_mmap` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_database` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_reload_interval_seconds` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_firewall_trace` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysql_native_password_proxy_users` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_bind_address` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_compression_algorithms` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_connect_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_deflate_default_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_deflate_max_client_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_document_id_unique_prefix` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_enable_hello_notice` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_idle_worker_thread_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_interactive_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_lz4_default_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_lz4_max_client_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_max_allowed_packet` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_max_connections` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_min_worker_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_port` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_port_open_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_read_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_socket` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_ca` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_capath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_cert` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_cipher` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_crl` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_crlpath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_ssl_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_wait_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_write_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_zstd_default_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `mysqlx_zstd_max_client_compression_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `named_pipe` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `named_pipe_full_access_group` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_allow_copying_alter_table` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_applier_allow_skip_epoch` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_autoincrement_prefetch_sz` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_batch_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_blob_read_batch_bytes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_blob_write_batch_bytes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_clear_apply_status` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_cluster_connection_pool` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_cluster_connection_pool_nodeids` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_conflict_role` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_data_node_neighbour` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_dbg_check_shares` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_default_column_format` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_deferred_constraints` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_distribution` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_eventbuffer_free_percent` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_eventbuffer_max_alloc` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_extra_logging` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_force_send` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_fully_replicated` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_index_stat_enable` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_index_stat_option` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_join_pushdown` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_apply_status` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_bin` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_binlog_index` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_cache_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_empty_epochs` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_empty_update` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_exclusive_reads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_fail_terminate` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_orig` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_compression` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_compression_level_zstd` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_dependency` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_transaction_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_update_as_write` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_update_minimal` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_log_updated_only` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_metadata_check` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_metadata_check_interval` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_metadata_sync` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_mgm_tls` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_optimization_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_optimized_node_selection` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_read_backup` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_recv_thread_activation_threshold` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_recv_thread_cpu_mask` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_replica_batch_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_replica_blob_write_batch_bytes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `Ndb_replica_max_replicated_epoch` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_report_thresh_binlog_epoch_slip` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_report_thresh_binlog_mem_usage` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_row_checksum` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_schema_dist_lock_wait_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_schema_dist_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_schema_dist_upgrade_allowed` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `Ndb_schema_participant_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_show_foreign_key_mock_tables` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_slave_conflict_role` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `Ndb_system_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_table_no_logging` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_table_temporary` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_tls_search_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_use_copying_alter_table` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_use_exact_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_use_transactions` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_version` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_version_string` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_wait_connected` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndb_wait_setup` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_database` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_max_bytes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_max_rows` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_offline` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_show_hidden` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_table_prefix` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ndbinfo_version` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_buffer_length` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_read_timeout` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_retry_count` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `net_write_timeout` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ngram_token_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `offline_mode` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `old_alter_table` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `open_files_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_prune_level` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_search_depth` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_switch` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_features` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_max_mem_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `optimizer_trace_offset` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `original_commit_timestamp` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `original_server_version` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `parser_max_mem_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `partial_revokes` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `password_history` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `password_require_current` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `password_reuse_interval` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_accounts_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_digests_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_error_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_stages_history_long_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_stages_history_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_statements_history_long_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_statements_history_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_transactions_history_long_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_transactions_history_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_waits_history_long_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_events_waits_history_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_hosts_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_cond_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_cond_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_digest_length` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_digest_sample_age` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_file_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_file_handles` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_file_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_index_stat` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_memory_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_metadata_locks` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_meter_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_metric_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_mutex_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_mutex_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_prepared_statements_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_program_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_rwlock_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_rwlock_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_socket_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_socket_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_sql_text_length` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_stage_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_statement_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_statement_stack` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_table_handles` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_table_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_table_lock_stat` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_thread_classes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_max_thread_instances` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_session_connect_attrs_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_setup_actors_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_setup_objects_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_show_processlist` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `performance_schema_users_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `persist_only_admin_x509_subject` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `persist_sensitive_variables_in_plaintext` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `persisted_globals_load` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pid_file` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `plugin_dir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `port` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `preload_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `print_identified_with_as_hex` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `profiling` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `profiling_history_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `protocol_compression_algorithms` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `protocol_version` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `proxy_user` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pseudo_replica_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pseudo_slave_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `pseudo_thread_id` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `query_alloc_block_size` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `query_prealloc_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rand_seed1` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rand_seed2` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `range_alloc_block_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `range_optimizer_max_mem_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rbr_exec_mode` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `read_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `read_only` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `read_rnd_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `regexp_stack_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `regexp_time_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_basename` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_index` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_purge` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_recovery` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `relay_log_space_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_allow_batching` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_checkpoint_group` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_checkpoint_period` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_compressed_protocol` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_exec_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_load_tmpdir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_max_allowed_packet` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_net_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_parallel_type` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_parallel_workers` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_pending_jobs_size_max` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_preserve_commit_order` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_skip_errors` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_sql_verify_checksum` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_transaction_retries` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replica_type_conversions` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replication_optimize_for_static_plugin_config` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `replication_sender_observe_commit_only` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_host` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_password` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_port` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `report_user` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `require_row_format` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `require_secure_transport` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `restrict_fk_on_non_standard_key` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `resultset_metadata` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rewriter_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rewriter_enabled_for_threads_without_privilege_checks` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rewriter_verbose` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_read_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_trace_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_wait_for_slave_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_wait_no_slave` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_master_wait_point` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_replica_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_replica_trace_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_slave_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_slave_trace_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_trace_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_wait_for_replica_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_wait_no_replica` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_semi_sync_source_wait_point` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_stop_replica_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `rpl_stop_slave_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `schema_definition_cache` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `secondary_engine_cost_threshold` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `secure_file_priv` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `select_into_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `select_into_disk_sync` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `select_into_disk_sync_delay` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `server_id` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `server_id_bits` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `server_uuid` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_gtids` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_schema` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_state_change` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_system_variables` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `session_track_transaction_info` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `set_operations_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_auto_generate_rsa_keys` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_private_key_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_proxy_users` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sha256_password_public_key_path` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `shared_memory` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `shared_memory_base_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `show_create_table_skip_secondary_engine` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `show_create_table_verbosity` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `show_gipk_in_create_table_and_information_schema` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_external_locking` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_name_resolve` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_networking` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_replica_start` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_show_database` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `skip_slave_start` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_allow_batching` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_checkpoint_group` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_checkpoint_period` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_compressed_protocol` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_exec_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_load_tmpdir` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_max_allowed_packet` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_net_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_parallel_type` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_parallel_workers` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_pending_jobs_size_max` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_preserve_commit_order` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_skip_errors` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_sql_verify_checksum` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_transaction_retries` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slave_type_conversions` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slow_launch_time` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slow_query_log` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `slow_query_log_file` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `socket` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sort_buffer_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `source_verify_checksum` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_auto_is_null` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_big_selects` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_buffer_result` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_generate_invisible_primary_key` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_log_bin` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_log_off` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_mode` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_notes` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_quote_show_create` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_replica_skip_counter` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_require_primary_key` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_safe_updates` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_select_limit` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_slave_skip_counter` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sql_warnings` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_ca` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_capath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_cert` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_cipher` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_crl` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_crlpath` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_fips_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_session_cache_mode` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `ssl_session_cache_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `statement_id` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `stored_program_cache` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `stored_program_definition_cache` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `super_read_only` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_binlog` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_master_info` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_relay_log` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_relay_log_info` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `sync_source_info` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `syseventlog.facility` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `syseventlog.include_pid` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `syseventlog.tag` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `system_time_zone` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_definition_cache` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_encryption_privilege_check` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_open_cache` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `table_open_cache_instances` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tablespace_definition_cache` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_reader_frequency_1` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_reader_frequency_2` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.metrics_reader_frequency_3` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_bsp_max_export_batch_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_bsp_max_queue_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_bsp_schedule_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_certificates` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_cipher` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_cipher_suite` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_client_certificates` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_client_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_compression` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_endpoint` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_headers` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_max_tls` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_min_tls` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_protocol` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_metrics_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_certificates` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_cipher` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_cipher_suite` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_client_certificates` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_client_key` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_compression` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_endpoint` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_headers` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_max_tls` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_min_tls` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_protocol` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_exporter_otlp_traces_timeout` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_log_level` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.otel_resource_attributes` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.query_text_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `telemetry.trace_enabled` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `temptable_max_mmap` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `temptable_max_ram` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `temptable_use_mmap` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `terminology_use_previous` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_cache_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_handling` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_algorithm` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_dedicated_listeners` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_high_priority_connection` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_longrun_trx_limit` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_max_active_query_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_max_transactions_limit` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_max_unused_threads` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_prio_kickup_timer` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_query_threads_per_group` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_size` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_stall_limit` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_pool_transaction_delay` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `thread_stack` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `time_zone` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `timestamp` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tls_certificates_enforced_validation` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tls_ciphersuites` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tls_version` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tmp_table_size` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `tmpdir` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_alloc_block_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_allow_batching` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_isolation` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Task 21 start spec assumes MySQL's default repeatable-read compatibility state and documents future `WITH CONSISTENT SNAPSHOT` warning behavior; variable exposure and isolation enforcement remain unimplemented. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `transaction_prealloc_size` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `transaction_read_only` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. | Task 21 start spec documents `START TRANSACTION READ ONLY` and future session/default access-mode integration; variable exposure and `SET` behavior remain unimplemented. See [transaction statements spec](docs/specs/transaction-statements/specs.md). |
| `unique_checks` | ❌ | high | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `updatable_views_with_limit` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `use_secondary_engine` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_check_user_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_dictionary_file` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_length` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_mixed_case_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_number_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_policy` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password_special_char_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.changed_characters_percentage` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.check_user_name` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.dictionary_file` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.length` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.mixed_case_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.number_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.policy` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `validate_password.special_char_count` | ❌ | low | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_comment` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_compile_machine` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_compile_os` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_compile_zlib` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_tokens_session` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `version_tokens_session_number` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `wait_timeout` | ❌ | top | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `warning_count` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `windowing_use_high_precision` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |
| `xa_detach_on_prepare` | ❌ | medium | Expose MySQL-compatible value, scope, mutability metadata, SET behavior, and diagnostics. |  |

### 7.4 Server status variables

The exact value shape, counter lifetime, session/global visibility, optional plugin/build availability, and `SHOW STATUS`/`performance_schema` exposure must be verified per variable.

| Variable | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `Aborted_clients` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Aborted_connects` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Acl_cache_items_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_current_size` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_direct_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_event_max_drop_size` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events_filtered` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_events_written` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_total_size` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Audit_log_write_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Authentication_ldap_sasl_supported_methods` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_cache_disk_use` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_cache_use` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_stmt_cache_disk_use` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Binlog_stmt_cache_use` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Bytes_received` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Bytes_sent` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Caching_sha2_password_rsa_public_key` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_admin_commands` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_db` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_event` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_function` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_procedure` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_resource_group` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_server` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_table` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_tablespace` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_user` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_alter_user_default_role` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_analyze` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_assign_to_keycache` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_begin` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_binlog` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_call_procedure` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_change_db` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_change_repl_filter` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_change_replication_source` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_check` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_checksum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_clone` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_commit` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_db` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_event` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_function` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_index` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_procedure` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_resource_group` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_role` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_server` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_table` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_trigger` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_udf` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_user` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_create_view` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_dealloc_sql` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_delete` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_delete_multi` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_do` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_db` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_event` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_function` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_index` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_procedure` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_resource_group` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_role` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_server` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_table` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_trigger` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_user` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_drop_view` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_empty_query` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_execute_sql` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_explain_other` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_flush` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_get_diagnostics` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_grant` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_grant_roles` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_group_replication_start` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_group_replication_stop` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_ha_close` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_ha_open` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_ha_read` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_help` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_insert` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_insert_select` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_install_component` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_install_plugin` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_kill` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_load` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_lock_tables` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_optimize` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_preload_keys` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_prepare_sql` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_purge` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_purge_before_date` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_release_savepoint` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rename_table` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rename_user` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_repair` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replace` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replace_select` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replica_start` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_replica_stop` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_reset` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_resignal` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_restart` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_revoke` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_revoke_all` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_revoke_roles` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rollback` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_rollback_to_savepoint` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_savepoint` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_select` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_set_option` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_set_resource_group` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_set_role` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_authors` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_binary_log_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_binlog_events` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_binlogs` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_charsets` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_collations` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_contributors` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_db` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_event` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_func` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_proc` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_table` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_trigger` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_create_user` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_databases` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_engine_logs` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_engine_mutex` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_engine_status` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_errors` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_events` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_fields` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_function_code` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_function_status` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_grants` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_keys` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_ndb_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_open_tables` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_plugins` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_privileges` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_procedure_code` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_procedure_status` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_processlist` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_profile` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_profiles` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_relaylog_events` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_replica_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_replicas` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_status` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_storage_engines` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_table_status` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_tables` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_triggers` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_variables` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_show_warnings` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_shutdown` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_signal` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_close` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_execute` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_fetch` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_prepare` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_reprepare` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_reset` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_stmt_send_long_data` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_truncate` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_uninstall_component` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_uninstall_plugin` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_unlock_tables` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_update` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_update_multi` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_commit` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_end` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_prepare` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_recover` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_rollback` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Com_xa_start` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Compression` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Compression_algorithm` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Compression_level` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_control_delay_generated` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_accept` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_internal` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_max_connections` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_peer_address` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_select` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connection_errors_tcpwrap` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Connections` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Created_tmp_disk_tables` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Created_tmp_files` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Created_tmp_tables` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_ca` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_capath` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_cert` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_cipher` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_ciphersuites` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_crl` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_crlpath` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_key` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Current_tls_version` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Delayed_errors` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Delayed_insert_threads` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Delayed_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Deprecated_use_i_s_processlist_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Deprecated_use_i_s_processlist_last_timestamp` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `dragnet.Status` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_buffered_bytes` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_buffered_events` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_expired_events` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Error_log_latest_write` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_access_denied` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_access_granted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_access_suspicious` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Firewall_cached_entries` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Flush_commands` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Global_connection_memory` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_all_consensus_proposals_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_all_consensus_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_certification_garbage_collector_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_certification_garbage_collector_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_consensus_bytes_received_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_consensus_bytes_sent_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_control_messages_sent_bytes_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_control_messages_sent_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_control_messages_sent_roundtrip_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_data_messages_sent_bytes_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_data_messages_sent_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_data_messages_sent_roundtrip_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_empty_consensus_proposals_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_extended_consensus_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_active_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_last_throttle_timestamp` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_flow_control_throttle_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_last_consensus_end_timestamp` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_total_messages_sent_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_sync_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_sync_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_termination_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_after_termination_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_before_begin_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Gr_transactions_consistency_before_begin_time_sum` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_commit` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_delete` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_discover` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_external_lock` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_mrr_init` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_prepare` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_first` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_key` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_last` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_next` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_prev` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_rnd` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_read_rnd_next` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_rollback` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_savepoint` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_savepoint_rollback` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_update` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Handler_write` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_bytes_data` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_bytes_dirty` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_dump_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_load_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_data` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_dirty` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_flushed` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_free` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_latched` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_misc` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_pages_total` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_ahead` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_ahead_evicted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_ahead_rnd` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_read_requests` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_reads` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_resize_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_resize_status_code` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_resize_status_progress` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_wait_free` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_buffer_pool_write_requests` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_fsyncs` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_pending_fsyncs` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_pending_reads` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_pending_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_read` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_reads` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_data_written` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_dblwr_pages_written` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_dblwr_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_have_atomic_builtins` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_log_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_log_write_requests` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_log_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_num_open_files` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_fsyncs` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_pending_fsyncs` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_pending_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_os_log_written` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_page_size` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_pages_created` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_pages_read` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_pages_written` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_capacity_resized` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_checkpoint_lsn` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_current_lsn` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_enabled` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_flushed_to_disk_lsn` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_logical_size` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_physical_size` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_read_only` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_resize_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_redo_log_uuid` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_current_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_time_avg` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_time_max` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_row_lock_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_deleted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_inserted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_read` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_rows_updated` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_deleted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_inserted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_read` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_system_rows_updated` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_truncated_status_writes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_active` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_explicit` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_implicit` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Innodb_undo_tablespaces_total` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_blocks_not_flushed` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_blocks_unused` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_blocks_used` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_read_requests` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_reads` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_write_requests` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Key_writes` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Last_query_cost` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Last_query_partial_plans` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Locked_connects` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_execution_time_exceeded` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_execution_time_set` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_execution_time_set_failed` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_used_connections` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Max_used_connections_time` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `mecab_charset` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_aborted_clients` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_address` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_received` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_received_compressed_payload` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_received_uncompressed_frame` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_sent` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_sent_compressed_payload` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_bytes_sent_uncompressed_frame` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_compression_algorithm` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_compression_level` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connection_accept_errors` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connection_errors` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connections_accepted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connections_closed` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_connections_rejected` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_create_view` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_delete` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_drop_view` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_find` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_insert` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_modify_view` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_crud_update` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_cursor_close` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_cursor_fetch` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_cursor_open` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_errors_sent` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_errors_unknown_message_type` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_expect_close` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_expect_open` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_init_error` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_messages_sent` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notice_global_sent` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notice_other_sent` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notice_warning_sent` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_notified_by_group_replication` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_port` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_prep_deallocate` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_prep_execute` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_prep_prepare` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_rows_sent` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_accepted` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_closed` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_fatal_error` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_killed` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_sessions_rejected` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_socket` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_accept_renegotiates` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_accepts` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_active` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_cipher` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_cipher_list` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_ctx_verify_depth` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_ctx_verify_mode` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_finished_accepts` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_server_not_after` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_server_not_before` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_verify_depth` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_verify_mode` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_ssl_version` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_create_collection` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_create_collection_index` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_disable_notices` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_drop_collection` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_drop_collection_index` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_enable_notices` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_ensure_collection` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_execute_mysqlx` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_execute_sql` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_execute_xplugin` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_get_collection_options` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_kill_client` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_list_clients` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_list_notices` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_list_objects` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_modify_collection_options` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_stmt_ping` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_worker_threads` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Mysqlx_worker_threads_active` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_deferred_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_forced_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_adaptive_send_unforced_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_received_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_bytes_sent_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_bytes_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_bytes_count_injector` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_data_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_data_count_injector` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_nondata_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_event_nondata_count_injector` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pk_op_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_pruned_scan_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_range_scan_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_read_row_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_scan_batch_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_table_scan_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_abort_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_close_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_commit_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_local_read_row_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_trans_start_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_uk_op_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_exec_complete_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_meta_request_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_nanos_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count_replica` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_api_wait_scan_result_count_slave` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_cluster_node_id` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_config_from_host` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_config_from_port` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_config_generation` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch_trans` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch2` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_epoch2_trans` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max_del_win` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max_del_win_ins` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_max_ins` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_fn_old` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_last_conflict_epoch` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_last_stable_epoch` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_reflected_op_discard_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_reflected_op_prepare_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_refresh_op_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_conflict_commit_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_detect_iter_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_reject_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_row_conflict_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_conflict_trans_row_reject_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_epoch_delete_delete_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_execute_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_fetch_table_stats` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_last_commit_epoch_server` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_last_commit_epoch_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_metadata_detected_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_metadata_excluded_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_metadata_synced_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_number_of_data_nodes` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pruned_scan_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_queries_defined` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_queries_dropped` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_queries_executed` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_pushed_reads` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_scan_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_slave_max_replicated_epoch` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ndb_trans_hint_count_session` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Not_flushed_delayed_rows` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ongoing_anonymous_gtid_violating_transaction_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ongoing_anonymous_transaction_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ongoing_automatic_gtid_violating_transaction_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_files` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_streams` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_table_definitions` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Open_tables` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Opened_files` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Opened_table_definitions` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Opened_tables` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_accounts_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_cond_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_cond_instances_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_digest_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_file_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_file_handles_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_file_instances_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_hosts_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_index_stat_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_locker_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_memory_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_metadata_lock_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_meter_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_metric_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_mutex_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_mutex_instances_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_nested_statement_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_prepared_statements_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_program_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_rwlock_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_rwlock_instances_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_session_connect_attrs_longest_seen` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_session_connect_attrs_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_socket_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_socket_instances_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_stage_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_statement_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_table_handles_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_table_instances_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_table_lock_stat_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_thread_classes_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_thread_instances_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Performance_schema_users_lost` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Prepared_stmt_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Queries` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Questions` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Replica_open_temp_tables` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Resource_group_supported` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_number_loaded_rules` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_number_reloads` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_number_rewritten_queries` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rewriter_reload_error` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_clients` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_net_avg_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_net_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_net_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_no_times` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_no_tx` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_timefunc_failures` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_tx_avg_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_tx_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_tx_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_wait_pos_backtraverse` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_wait_sessions` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_master_yes_tx` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_replica_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_slave_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_clients` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_net_avg_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_net_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_net_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_no_times` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_no_tx` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_status` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_timefunc_failures` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_tx_avg_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_tx_wait_time` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_tx_waits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_wait_pos_backtraverse` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_wait_sessions` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rpl_semi_sync_source_yes_tx` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Rsa_public_key` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Secondary_engine_execution_count` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_full_join` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_full_range_join` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_range` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_range_check` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Select_scan` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slave_open_temp_tables` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slave_rows_last_search_algorithm_used` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slow_launch_threads` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Slow_queries` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_merge_passes` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_range` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_rows` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Sort_scan` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_accept_renegotiates` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_accepts` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_callback_cache_hits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_cipher` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_cipher_list` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_client_connects` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_connect_renegotiates` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_ctx_verify_depth` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_ctx_verify_mode` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_default_timeout` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_finished_accepts` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_finished_connects` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_server_not_after` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_server_not_before` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_hits` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_misses` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_mode` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_overflows` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_size` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_timeout` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_session_cache_timeouts` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_sessions_reused` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_used_session_cache_entries` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_verify_depth` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_verify_mode` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Ssl_version` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_locks_immediate` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_locks_waited` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_open_cache_hits` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_open_cache_misses` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Table_open_cache_overflows` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tc_log_max_pages_used` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tc_log_page_size` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tc_log_page_waits` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Telemetry_metrics_supported` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Telemetry_traces_supported` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `telemetry.live_sessions` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_cached` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_connected` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_created` | ❌ | high | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Threads_running` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tls_library_version` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Tls_sni_server_name` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Uptime` | ❌ | top | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `Uptime_since_flush_status` | ❌ | medium | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password_dictionary_file_last_parsed` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password_dictionary_file_words_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password.dictionary_file_last_parsed` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |
| `validate_password.dictionary_file_words_count` | ❌ | low | Expose MySQL-compatible value and counter/update semantics, or a documented embedded-compatible zero/empty value. |  |

## 8. Wire protocol and client API surface

### 8.1 Connection, authentication, and packets

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| Protocol 10 initial handshake | ❌ | top | Server greeting, connection id, auth plugin data, status flags, charset, and capability negotiation. |  |
| Capability flags | ❌ | top | Client/server capability negotiation and rejection semantics. |  |
| SSLRequest and TLS upgrade | ❌ | high | TLS negotiation, failure modes, and capability interactions. |  |
| Handshake Response 41 | ❌ | top | Username, auth response, database, auth plugin name, connection attributes, and charset. |  |
| `caching_sha2_password` | ❌ | high | Default MySQL 8 authentication plugin packet flow and RSA/TLS behavior. |  |
| `sha256_password` | ❌ | high | Password exchange packet flow and diagnostics. |  |
| `mysql_native_password` | ❌ | high | Deprecated plugin compatibility and MySQL 8.4 behavior. |  |
| Auth switch request/response | ❌ | high | Plugin switching packet flow. |  |
| Auth more data | ❌ | high | Multi-step authentication packet flow. |  |
| OK packet | ❌ | top | Affected rows, last insert id, status flags, warnings, session-state tracking, and EOF deprecation. |  |
| ERR packet | ❌ | top | Error code, SQLSTATE, message text, and fatal/nonfatal behavior. |  |
| EOF packet compatibility | ❌ | high | Legacy EOF packet behavior when CLIENT_DEPRECATE_EOF is not set. |  |
| Result set metadata | ❌ | top | Column count, column definitions, flags, charsets, decimals, schema/table/origin names, and EOF/OK termination. | Task 23 implements shared result-column descriptors and C API accessors for current scalar and one-table `SELECT` output, including labels, origin names, field type, flags, length, decimals, charset id, and nullability. MySQL wire-protocol column-definition packet emission, optional metadata negotiation, EOF/OK termination behavior, prepared-statement metadata, and materialized `max_length` remain deferred. See [result metadata and expression labels spec](docs/specs/result-metadata-expression-labels/specs.md). |
| Text result rows | ❌ | top | Text protocol row encoding, NULLs, character sets, and length-encoded values. |  |
| Binary result rows | ❌ | high | Prepared statement row encoding, null bitmap, type encodings, and unsigned flags. |  |
| LOCAL INFILE request | ❌ | high | Client file upload request flow and security controls. |  |
| Compression protocol | ❌ | medium | Compressed packet framing and capability negotiation. |  |
| Zstandard compression | ❌ | medium | zstd compression negotiation and packet behavior. |  |
| Connection attributes | ❌ | high | Attribute parsing and exposure in performance_schema/session tables. |  |
| Session state tracking | ❌ | high | Schema, system variables, GTIDs, transaction state, and state-change notices. |  |

### 8.2 Command packets

| Command | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| `COM_SLEEP` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_QUIT` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_INIT_DB` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_QUERY` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_FIELD_LIST` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CREATE_DB` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_DROP_DB` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_UNUSED_2` | ❌ | low | Preserve MySQL 8.4.9 command-number handling for the removed `COM_REFRESH` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_UNUSED_1` | ❌ | low | Preserve MySQL 8.4.9 command-number handling for the removed `COM_SHUTDOWN` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_STATISTICS` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_UNUSED_4` | ❌ | low | Preserve MySQL 8.4.9 command-number handling for the removed `COM_PROCESS_INFO` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_CONNECT` | ❌ | medium | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_UNUSED_5` | ❌ | low | Preserve MySQL 8.4.9 command-number handling for the removed `COM_PROCESS_KILL` slot, including unsupported-command diagnostics. | Removed command slot. |
| `COM_DEBUG` | ❌ | medium | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_PING` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_TIME` | ❌ | medium | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_DELAYED_INSERT` | ❌ | medium | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CHANGE_USER` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_BINLOG_DUMP` | ❌ | low | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_TABLE_DUMP` | ❌ | low | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CONNECT_OUT` | ❌ | medium | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_REGISTER_SLAVE` | ❌ | low | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_PREPARE` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_EXECUTE` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_SEND_LONG_DATA` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_CLOSE` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_RESET` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_SET_OPTION` | ❌ | top | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_STMT_FETCH` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_DAEMON` | ❌ | low | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_BINLOG_DUMP_GTID` | ❌ | low | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_RESET_CONNECTION` | ❌ | high | Implement MySQL-compatible command packet parsing, response shape, status flags, and errors. |  |
| `COM_CLONE` | ❌ | low | Implement MySQL-compatible clone command packet parsing, plugin handoff behavior, status flags, and errors. |  |
| `COM_SUBSCRIBE_GROUP_REPLICATION_STREAM` | ❌ | low | Implement MySQL-compatible Group Replication stream subscription command, privilege checks, handoff behavior, and errors. |  |

## 9. Error, warning, and result semantics

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| Error code catalog | ❌ | top | Use MySQL 8.4 error numbers, SQLSTATE values, and message text where practical. |  |
| Warning code catalog | ❌ | top | Use MySQL 8.4 warning numbers, SQLSTATE values, message text, and ordering. |  |
| Diagnostics area | ❌ | top | Statement diagnostics, condition areas, row count, and GET DIAGNOSTICS integration. |  |
| Strict-mode errors | ❌ | top | Escalation from warnings to errors under strict SQL modes. |  |
| IGNORE warning demotion | ❌ | top | DML IGNORE conversion, duplicate, constraint, and truncation behavior. | Started for `INSERT IGNORE`; see [INSERT IGNORE spec](docs/specs/insert-ignore/specs.md). Broader `UPDATE IGNORE`, `DELETE IGNORE`, `LOAD DATA IGNORE`, and `CREATE TABLE ... SELECT IGNORE` behavior remains deferred. |
| Metadata flags | ❌ | top | Column flags for NOT NULL, PRI/UNI/MUL, BLOB, UNSIGNED, ZEROFILL, BINARY, ENUM, AUTO_INCREMENT, TIMESTAMP, SET, and NUM. |  |
| Result ordering guarantees | 🟡 | top | Match MySQL ordering only where SQL semantics or MySQL behavior require it. | Implemented for Task 18's one-table `SELECT ... ORDER BY ... LIMIT/OFFSET` subset and the Task 25 aggregate grouping slice with deterministic tests only when explicit sort keys fully define row order. Equal-key ordering remains intentionally unspecified, and joins, set operations, optimizer-sensitive order preservation, and full collation edge cases remain deferred. See [ORDER BY, LIMIT, and OFFSET spec](docs/specs/order-limit-offset/specs.md) and [aggregate functions and grouping spec](docs/specs/aggregate-grouping/specs.md). |
| Floating-point edge cases | ❌ | high | NaN/Inf handling, division, rounding, comparison, and platform-sensitive behavior. |  |
| Temporal edge cases | ❌ | top | Zero dates, leap days, DST, fractional truncation/rounding, and timezone table behavior. |  |
| JSON edge cases | ❌ | top | Duplicate keys, ordering, binary JSON storage-visible behavior, path errors, and partial update metadata. |  |
| Spatial edge cases | ❌ | medium | SRID mismatch, geographic vs Cartesian calculations, invalid geometry handling, and units. |  |
| Privilege-sensitive metadata | ❌ | high | Metadata visibility affected by grants and DEFINER/INVOKER context. |  |

## 10. Embedded-design compatibility decisions

Some MySQL server features do not naturally map to an in-process single-file database. They still belong in the compatibility matrix because applications may issue the syntax or inspect related metadata. Each row needs an explicit design decision before it can move out of `❌`.

| Feature | Status | Priority | Target behavior | Implementation notes |
| --- | --- | --- | --- | --- |
| Replication and binary logs | ❌ | low | Decide between unsupported errors, no-op placeholders, metadata stubs, or local change streams. |  |
| Account management and privileges | ❌ | high | Decide how much auth/privilege syntax is accepted and how metadata is represented without a server account model. |  |
| Resource groups | ❌ | low | Decide embedded diagnostics for CPU/thread scheduling features. |  |
| Components and plugins | ❌ | low | Decide loadable component/plugin syntax behavior and metadata exposure. |  |
| Server lifecycle commands | ❌ | low | Decide diagnostics for RESTART, SHUTDOWN, CLONE, and backup-lock operations. |  |
| Storage engines | ❌ | high | Decide how ENGINE clauses, SHOW ENGINES, and engine-specific metadata map to SQLite-backed storage. |  |
| `InnoDB` engine surface | ❌ | high | Decide how MySQL's default engine metadata, syntax, and diagnostics map onto SQLite-backed storage. |  |
| `MyISAM` engine surface | ❌ | medium | Decide parser, metadata, SHOW ENGINES, and unsupported-feature diagnostics. |  |
| `MEMORY` engine surface | ❌ | high | Decide temporary/in-memory table compatibility and diagnostics. |  |
| `CSV` engine surface | ❌ | medium | Decide log-table and ENGINE=CSV compatibility behavior. |  |
| `ARCHIVE` engine surface | ❌ | low | Decide archive-engine syntax and diagnostics. |  |
| `BLACKHOLE` engine surface | ❌ | low | Decide blackhole-engine syntax and embedded-compatible behavior. |  |
| `MERGE` engine surface | ❌ | low | Decide MERGE/MyISAM union table syntax and diagnostics. |  |
| `FEDERATED` engine surface | ❌ | low | Decide foreign-server table syntax, mysql.servers metadata, and diagnostics. |  |
| `NDB` engine surface | ❌ | low | Decide NDB-specific DDL syntax, metadata tables, and diagnostics. |  |
| `PERFORMANCE_SCHEMA` engine surface | ❌ | medium | Decide metadata and diagnostics for performance_schema engine tables. |  |
| Tablespaces and logfile groups | ❌ | low | Decide syntax acceptance and diagnostics for non-single-file storage constructs. |  |
| Performance Schema | ❌ | medium | Decide which tables expose real embedded runtime metrics versus documented empty placeholders. |  |
| sys schema | ❌ | medium | Decide which objects are useful over MyLite metadata and which should be documented as empty or unsupported. |  |
| File import/export | ❌ | high | Decide secure embedded behavior for LOAD DATA, LOAD XML, SELECT INTO OUTFILE, and SELECT INTO DUMPFILE. |  |
| User-defined/loadable functions | ❌ | medium | Decide C-extension registration model, security boundaries, and binary size policy. |  |
| X Protocol and Document Store | ❌ | low | Decide whether MySQL X Protocol and CRUD-style document APIs are out of scope, stubbed, or implemented. |  |

## 11. Test expectations

- Every implemented row needs MySQL 8.4.9 comparison tests.
- Tests must cover normal results, errors, warnings, metadata, affected rows, inserted ids, session state, and side effects.
- Syntax-only compatibility must still test parser acceptance and the exact warning/error/placeholder behavior.
- Feature work should add focused guide documentation when behavior is subtle enough that future maintainers need the design rationale.
- When MySQL behavior is platform-dependent or storage-engine-dependent, tests should document the chosen MyLite contract and the observed MySQL baseline.
