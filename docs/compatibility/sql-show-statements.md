# SQL SHOW statements

MySQL SHOW statement result shapes, filters, privileges, and compatibility diagnostics.

| SHOW statement | Status | Notes |
| --- | --- | --- |
| `SHOW BINARY LOG STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW BINARY LOGS` | ❌ | Result shape, filters, privileges |
| `SHOW BINLOG EVENTS` | ❌ | Result shape, filters, privileges |
| `SHOW CHARACTER SET` / `SHOW CHARSET` | 🟡 | Limited static `utf8mb4` row with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `WHERE`, alternate charsets, privileges, or `INFORMATION_SCHEMA` |
| `SHOW COLLATION` | 🟡 | Limited static `utf8mb4_0900_ai_ci` row with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `WHERE`, alternate collations, privileges, or `INFORMATION_SCHEMA` |
| `SHOW COLUMNS` / `SHOW FIELDS` | 🟡 | Limited descriptor-driven column listing for persistent base tables; supports `FROM`/`IN`, schema-qualified targets, explicit schema forms, and `LIKE 'pattern'` filters with `Field`, `Type`, `Null`, `Key`, `Default`, and `Extra`; no NUL-producing pattern escapes, `FULL`, `EXTENDED`, `WHERE`, views, privileges, indexes, defaults, hidden columns, or `INFORMATION_SCHEMA` |
| `SHOW COUNT(*) ERRORS` | ❌ | Result shape, filters, privileges |
| `SHOW COUNT(*) WARNINGS` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Limited descriptor-driven schema DDL rendering with MySQL 8.4.9 columns and fixed default charset/collation/encryption text for current optionless schema descriptors; no `IF NOT EXISTS`, schema options, privileges, system schemas, or `sql_quote_show_create` |
| `SHOW CREATE EVENT` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE FUNCTION` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE PROCEDURE` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE TABLE` | 🟡 | Limited descriptor-driven MySQL-style DDL for persistent base tables with current integer-family descriptors and nullability; fixed InnoDB/utf8mb4 suffix; no views, temporary tables, indexes, defaults, constraints, generated columns, auto-increment, privileges, or `sql_quote_show_create` |
| `SHOW CREATE TRIGGER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE USER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE VIEW` | ❌ | Result shape, filters, privileges |
| `SHOW DATABASES` / `SHOW SCHEMAS` | 🟡 | Limited descriptor-driven catalog schema listing with `LIKE 'pattern'` filters; no NUL-producing pattern escapes, `WHERE`, system schemas, or privileges |
| `SHOW ENGINE` | ❌ | Subcommands and result shape |
| `SHOW ENGINE LOGS` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINE MUTEX` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINE STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW ENGINES` | 🟡 | Limited `SHOW [STORAGE] ENGINES` exposes one embedded InnoDB default row with MySQL 8.4.9 column labels; no alternate engines, filters, privileges, plugins, or `INFORMATION_SCHEMA.ENGINES` |
| `SHOW ERRORS` | ❌ | Result shape, filters, privileges |
| `SHOW EVENTS` | 🟡 | Limited empty event introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` filters; unknown explicit schemas are empty successes; no NUL-producing pattern escapes, event descriptors, event rows, event DDL, `SHOW CREATE EVENT`, `WHERE`, Event Scheduler, privileges, or `INFORMATION_SCHEMA.EVENTS` |
| `SHOW FUNCTION CODE` | ❌ | Debug-only routine bytecode listing |
| `SHOW FUNCTION STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW GRANTS` | ❌ | Result shape, filters, privileges |
| `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` | 🟡 | Limited descriptor-resolved persistent base-table introspection with MySQL 8.4.9 column labels and zero rows for current no-index tables; no index descriptors, indexed rows, `EXTENDED`, `WHERE`, temporary tables, views, privileges, or statistics |
| `SHOW MASTER STATUS` | ❌ | Removed; use SHOW BINARY LOG STATUS |
| `SHOW OPEN TABLES` | ❌ | Result shape, filters, privileges |
| `SHOW PARSE_TREE` | ❌ | Conditional parse-tree debug output |
| `SHOW PLUGINS` | ❌ | Result shape, filters, privileges |
| `SHOW PRIVILEGES` | ❌ | Result shape, filters, privileges |
| `SHOW PROCEDURE CODE` | ❌ | Debug-only routine bytecode listing |
| `SHOW PROCEDURE STATUS` | ❌ | Result shape, filters, privileges |
| `SHOW PROCESSLIST` | ❌ | Result shape, filters, privileges |
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
| `SHOW WARNINGS` | ❌ | Result shape, filters, privileges |

[Back to compatibility overview](../../COMPATIBILITY.md)
