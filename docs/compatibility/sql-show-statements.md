# SQL SHOW statements

MySQL SHOW statement result shapes, filters, privileges, and compatibility diagnostics.

| SHOW statement | Status | Notes |
| --- | --- | --- |
| `SHOW BINARY LOG STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW BINARY LOGS` | ❌ | Result shape, filters, privileges |
| `SHOW BINLOG EVENTS` | ❌ | Result shape, filters, privileges |
| `SHOW CHARACTER SET` / `SHOW CHARSET` | 🟡 | Limited static `utf8mb4` row with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `WHERE`, alternate charsets, privileges, or `INFORMATION_SCHEMA` |
| `SHOW COLLATION` | 🟡 | Limited static `utf8mb4_0900_ai_ci` row with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `WHERE`, alternate collations, privileges, or `INFORMATION_SCHEMA` |
| `SHOW COLUMNS` / `SHOW FIELDS` | 🟡 | Limited descriptor-driven column listing for persistent base tables; supports `FROM`/`IN`, schema-qualified targets, explicit schema forms, and `LIKE 'pattern'` filters with `Field`, `Type`, `Null`, `Key`, SQL `NULL` `Default`, and `Extra`; no NUL-producing pattern escapes, `FULL`, `EXTENDED`, `WHERE`, views, privileges, indexes, non-`NULL` defaults, expression defaults, hidden columns, or `INFORMATION_SCHEMA` |
| `SHOW COUNT(*) ERRORS` | 🟡 | Limited previous-statement error-condition count with MySQL 8.4.9 column label `@@session.error_count`; counts MyLite's previous error condition only; related scalar `@@error_count` is limited separately; no notes, `max_error_count`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW COUNT(*) WARNINGS` | 🟡 | Limited previous-statement diagnostics count with MySQL 8.4.9 column label; counts MyLite's previous error condition plus stored warning/note records; missing-schema `DROP DATABASE IF EXISTS` intentionally leaves no stored count after reporting a statement warning count; no `max_error_count`, mutable `sql_notes`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Limited descriptor-driven schema DDL rendering with MySQL 8.4.9 columns, fixed default charset/collation/encryption text, and fixed enabled `sql_quote_show_create` quoting for current optionless schema descriptors; no `IF NOT EXISTS`, schema options, privileges, system schemas, mutable quote-control state, or disabled rendering |
| `SHOW CREATE EVENT` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE FUNCTION` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE PROCEDURE` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE TABLE` | 🟡 | Limited descriptor-driven MySQL-style DDL for persistent base tables with current integer-family descriptors, nullability, nullable-column `DEFAULT NULL` rendering, fixed InnoDB/utf8mb4 suffix, and fixed enabled `sql_quote_show_create` quoting; no views, temporary tables, indexes, non-`NULL` defaults, expression defaults, constraints, generated columns, auto-increment, privileges, mutable quote-control state, or disabled rendering |
| `SHOW CREATE TRIGGER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE USER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE VIEW` | ❌ | Result shape, filters, privileges |
| `SHOW DATABASES` / `SHOW SCHEMAS` | 🟡 | Limited descriptor-driven catalog schema listing with `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `WHERE`, system schemas, or privileges |
| `SHOW ENGINE` | ❌ | Subcommands and result shape |
| `SHOW ENGINE LOGS` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINE MUTEX` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINE STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINES` | 🟡 | Limited `SHOW [STORAGE] ENGINES` exposes one embedded InnoDB default row with MySQL 8.4.9 column labels; no alternate engines, filters, privileges, plugins, or `INFORMATION_SCHEMA.ENGINES` |
| `SHOW ERRORS` | 🟡 | Limited previous-statement error-condition rows with `Level`, `Code`, and `Message`, plus unsigned decimal `LIMIT` slicing; reports MyLite's previous error condition only; related scalar `@@error_count` is limited separately; no warning/note rows, `WHERE`, `LIKE`, expression filters, `max_error_count`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW EVENTS` | 🟡 | Limited empty event introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; unknown explicit schemas are empty successes; no NUL-producing pattern escapes, event descriptors, event rows, event DDL, `SHOW CREATE EVENT`, `WHERE`, Event Scheduler, privileges, or `INFORMATION_SCHEMA.EVENTS` |
| `SHOW FUNCTION CODE` | ❌ | Debug-only routine bytecode listing |
| `SHOW FUNCTION STATUS` | 🟡 | Limited empty routine introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; global and default-schema independent; no NUL-producing pattern escapes, routine descriptors, routine rows, routine DDL, `SHOW CREATE FUNCTION`, `WHERE`, privileges, or `INFORMATION_SCHEMA.ROUTINES` |
| `SHOW GRANTS` | ❌ | Result shape, filters, privileges |
| `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` | 🟡 | Limited descriptor-resolved persistent base-table introspection with MySQL 8.4.9 column labels and zero rows for current no-index tables; no index descriptors, indexed rows, `EXTENDED`, `WHERE`, temporary tables, views, privileges, or statistics |
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
| `SHOW TABLE STATUS` | 🟡 | Limited descriptor-driven persistent base-table status rows with MySQL 8.4.9 column labels, `FROM`/`IN` schema forms, `LIKE 'pattern'` filters, exact physical row counts, and deterministic placeholder statistics; no views, temporary tables, `WHERE`, privileges, timestamps, auto-increment metadata, full storage statistics, or `INFORMATION_SCHEMA` |
| `SHOW TABLES` | 🟡 | Limited descriptor-driven `SHOW TABLES` with optional `FROM`/`IN` schema and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `FULL`, `EXTENDED`, `WHERE`, views, privileges, or temporary tables |
| `SHOW TRIGGERS` | 🟡 | Limited schema-resolved empty trigger introspection with MySQL 8.4.9 column labels, optional `FULL`, and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, trigger descriptors, trigger rows, trigger DDL, `SHOW CREATE TRIGGER`, `WHERE`, privileges, or `INFORMATION_SCHEMA.TRIGGERS` |
| `SHOW VARIABLES` | ❌ | Result shape, filters, privileges |
| `SHOW WARNINGS` | 🟡 | Limited previous-statement diagnostics rows with `Level`, `Code`, and `Message`, plus unsigned decimal `LIMIT` slicing; reports MyLite previous error conditions and stored warning/note records only; missing-schema `DROP DATABASE IF EXISTS` intentionally stores no warning row after reporting a statement warning count; no `WHERE`, `LIKE`, expression filters, `max_error_count`, mutable `sql_notes`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |

[Back to compatibility overview](../../COMPATIBILITY.md)
