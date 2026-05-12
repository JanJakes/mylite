# SQL SHOW statements

MySQL SHOW statement result shapes, filters, privileges, and compatibility diagnostics.

| SHOW statement | Status | Notes |
| --- | --- | --- |
| `SHOW BINARY LOG STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW BINARY LOGS` | ❌ | Result shape, filters, privileges |
| `SHOW BINLOG EVENTS` | ❌ | Result shape, filters, privileges |
| `SHOW CHARACTER SET` / `SHOW CHARSET` | 🟡 | Limited static `utf8mb4` row with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; related `INFORMATION_SCHEMA.CHARACTER_SETS` is a separate limited one-row synthetic catalog; no NUL-producing pattern escapes, `WHERE`, alternate charsets, or privileges |
| `SHOW COLLATION` | 🟡 | Limited static `utf8mb4_0900_ai_ci` row with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; related `INFORMATION_SCHEMA.COLLATIONS` is a separate limited one-row synthetic catalog; no NUL-producing pattern escapes, `WHERE`, alternate collations, or privileges |
| `SHOW COLUMNS` / `SHOW FIELDS` | 🟡 | Limited descriptor-driven column listing for persistent base tables; supports `FROM`/`IN`, schema-qualified targets, explicit schema forms, and `LIKE 'pattern'` filters with `Field`, integer, `decimal(M,D)`, `decimal(M,D) unsigned`, `float`, `float unsigned`, `double`, `double unsigned`, `date`, `time`, `datetime`, `timestamp`, `char(n)`, `varchar(n)`, and baseline `TEXT` family `Type`, `Null`, `Key` including `PRI` for current primary-key descriptor columns or first `NOT NULL` unique index when no primary key exists, `UNI` for supported unique-index columns, and `MUL` for supported nonunique secondary-index columns, SQL `NULL`, canonical integer, canonical decimal, canonical approximate, decoded `CHAR`/`VARCHAR` string, canonical date-string, canonical time-string, canonical datetime-string, or canonical timestamp-string `Default`, and `Extra`, including `auto_increment` for the current auto-increment descriptor and `INVISIBLE` for descriptor-invisible columns; no NUL-producing pattern escapes, `FULL`, `EXTENDED`, `WHERE`, views, privileges, unsupported index metadata, `TEXT`-family string defaults, warning-producing overlength string-default truncation, expression defaults, generated hidden columns, or full `INFORMATION_SCHEMA` parity |
| `SHOW COUNT(*) ERRORS` | 🟡 | Limited previous-statement error-condition count with MySQL 8.4.9 column label `@@session.error_count`; counts MyLite's previous error condition only; related scalar `@@error_count` is limited separately; no notes, `max_error_count`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW COUNT(*) WARNINGS` | 🟡 | Limited previous-statement diagnostics count with MySQL 8.4.9 column label; counts MyLite's previous error condition plus stored warning/note records; missing-schema `DROP DATABASE IF EXISTS` intentionally leaves no stored count after reporting a statement warning count; no `max_error_count`, mutable `sql_notes`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Limited descriptor-driven schema DDL rendering with MySQL 8.4.9 columns, fixed default charset/collation/encryption text, and fixed enabled `sql_quote_show_create` quoting for current optionless schema descriptors; no `IF NOT EXISTS`, schema options, privileges, system schemas, mutable quote-control state, or disabled rendering |
| `SHOW CREATE EVENT` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE FUNCTION` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE PROCEDURE` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE TABLE` | 🟡 | Limited descriptor-driven MySQL-style DDL for persistent base tables with current integer-family, exact `DECIMAL`, approximate `FLOAT`/`DOUBLE`, canonical `DATE`, canonical `TIME`, canonical `DATETIME`, canonical `TIMESTAMP`, `CHAR(0..255)`, `VARCHAR(0..16383)`, and baseline `TEXT` family descriptors, nullability, implicit nullable-column `DEFAULT NULL` rendering for non-`TEXT` descriptors, dropped-default columns with no default clause, canonical quoted integer, decimal, approximate, `CHAR`/`VARCHAR` string, date, time, datetime, and timestamp defaults except hidden auto-increment defaults created by `ALTER ... SET DEFAULT`, descriptor-invisible column comments, current integer-family primary-key descriptors, supported unique/nonunique secondary-index, and auto-increment rendering, fixed InnoDB/utf8mb4 suffix, and fixed enabled `sql_quote_show_create` quoting; no views, temporary tables, unsupported index attributes, `TEXT`-family string defaults, warning-producing overlength string-default truncation, expression defaults, named constraints, generated columns, privileges, mutable quote-control state, or disabled rendering |
| `SHOW CREATE TRIGGER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE USER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE VIEW` | ❌ | Result shape, filters, privileges |
| `SHOW DATABASES` / `SHOW SCHEMAS` | 🟡 | Limited descriptor-driven catalog schema listing with `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `WHERE`, system schemas, or privileges |
| `SHOW ENGINE` | ❌ | Subcommands and result shape |
| `SHOW ENGINE LOGS` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINE MUTEX` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINE STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINES` | 🟡 | Limited `SHOW [STORAGE] ENGINES` exposes one embedded InnoDB default row with MySQL 8.4.9 column labels; related `INFORMATION_SCHEMA.ENGINES` is a separate limited one-row synthetic catalog; no alternate engines, filters, privileges, or plugins |
| `SHOW ERRORS` | 🟡 | Limited previous-statement error-condition rows with `Level`, `Code`, and `Message`, plus unsigned decimal `LIMIT` slicing; reports MyLite's previous error condition only; related scalar `@@error_count` is limited separately; no warning/note rows, `WHERE`, `LIKE`, expression filters, `max_error_count`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW EVENTS` | 🟡 | Limited empty event introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; unknown explicit schemas are empty successes; no NUL-producing pattern escapes, event descriptors, event rows, event DDL, `SHOW CREATE EVENT`, `WHERE`, Event Scheduler, privileges, or `INFORMATION_SCHEMA.EVENTS` |
| `SHOW FUNCTION CODE` | ❌ | Debug-only routine bytecode listing |
| `SHOW FUNCTION STATUS` | 🟡 | Limited empty routine introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; global and default-schema independent; no NUL-producing pattern escapes, routine descriptors, routine rows, routine DDL, `SHOW CREATE FUNCTION`, `WHERE`, privileges, or `INFORMATION_SCHEMA.ROUTINES` |
| `SHOW GRANTS` | ❌ | Result shape, filters, privileges |
| `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` | 🟡 | Limited descriptor-resolved persistent base-table introspection with MySQL 8.4.9 column labels, zero rows for no-index tables, one `PRIMARY` BTREE row per ordered primary-key part, and one BTREE row per supported unique or nonunique secondary key part, including `Sub_part` for current nonunique string prefix parts; no fulltext/spatial/functional/descending index metadata, `EXTENDED`, `WHERE`, temporary tables, views, privileges, or real storage statistics |
| `SHOW MASTER STATUS` | ❌ | Removed; use SHOW BINARY LOG STATUS |
| `SHOW OPEN TABLES` | 🟡 | Limited embedded empty open-table introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; unknown explicit schemas are empty successes; no NUL-producing pattern escapes, table-cache rows, `In_use` counts, `Name_locked` state, temporary tables, `HANDLER`, table locks, `WHERE`, privileges, or performance-schema metadata |
| `SHOW PARSE_TREE` | ❌ | Conditional parse-tree debug output |
| `SHOW PLUGINS` | ❌ | Result shape, filters, privileges |
| `SHOW PRIVILEGES` | ❌ | Result shape, filters, privileges |
| `SHOW PROCEDURE CODE` | ❌ | Debug-only routine bytecode listing |
| `SHOW PROCEDURE STATUS` | 🟡 | Limited empty routine introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; global and default-schema independent; no NUL-producing pattern escapes, routine descriptors, routine rows, routine DDL, `SHOW CREATE PROCEDURE`, `WHERE`, privileges, or `INFORMATION_SCHEMA.ROUTINES` |
| `SHOW PROCESSLIST` | 🟡 | Limited current embedded-handle row for `SHOW [FULL] PROCESSLIST` with MySQL 8.4.9 columns, selected-schema `db`, `Info` truncation, and default process-list warning count; no server-wide threads, sleeping/background rows, filters, privileges, Performance Schema, `INFORMATION_SCHEMA.PROCESSLIST`, sys-schema views, or `KILL` |
| `SHOW PROFILE` | ❌ | Result shape, filters, privileges |
| `SHOW PROFILES` | ❌ | Result shape, filters, privileges |
| `SHOW RELAYLOG EVENTS` | ❌ | Result shape, filters, privileges |
| `SHOW REPLICA STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW REPLICAS` | ❌ | Result shape, filters, privileges |
| `SHOW STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW TABLE STATUS` | 🟡 | Limited descriptor-driven persistent base-table status rows with MySQL 8.4.9 column labels, `FROM`/`IN` schema forms, `LIKE 'pattern'` filters, exact physical row counts, descriptor-owned `Auto_increment` values for the current auto-increment subset, and deterministic placeholder statistics; no views, temporary tables, `WHERE`, privileges, timestamps, full storage statistics, or `INFORMATION_SCHEMA` |
| `SHOW TABLES` | 🟡 | Limited descriptor-driven `SHOW TABLES` with optional `FROM`/`IN` schema and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `FULL`, `EXTENDED`, `WHERE`, views, privileges, or temporary tables |
| `SHOW TRIGGERS` | 🟡 | Limited schema-resolved empty trigger introspection with MySQL 8.4.9 column labels, optional `FULL`, and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, trigger descriptors, trigger rows, trigger DDL, `SHOW CREATE TRIGGER`, `WHERE`, privileges, or `INFORMATION_SCHEMA.TRIGGERS` |
| `SHOW VARIABLES` | 🟡 | Limited runtime-owned rows for MyLite's current fixed system-variable registry with MySQL 8.4.9 column labels, optional `GLOBAL`/`SESSION`/`LOCAL` scope, and `LIKE 'pattern'` filters; no full MySQL variable catalog, `WHERE`, privileges, persisted variables, Performance Schema variable tables, or mutable state |
| `SHOW WARNINGS` | 🟡 | Limited previous-statement diagnostics rows with `Level`, `Code`, and `Message`, plus unsigned decimal `LIMIT` slicing; reports MyLite previous error conditions and stored warning/note records only; missing-schema `DROP DATABASE IF EXISTS` intentionally stores no warning row after reporting a statement warning count; no `WHERE`, `LIKE`, expression filters, `max_error_count`, mutable `sql_notes`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |

[Back to compatibility overview](../../COMPATIBILITY.md)
