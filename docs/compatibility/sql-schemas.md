# SQL schemas

Database/schema selection and DDL compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER DATABASE` / `ALTER SCHEMA` | ❌ | Schema default options |
| `CREATE DATABASE` / `CREATE SCHEMA` | 🟡 | Creates persistent MyLite catalog schemas with optional `IF NOT EXISTS`; existing-schema `IF NOT EXISTS` succeeds as a no-op with stored `Note 1007`, affected rows `1`, and no catalog or SQLite schema mutation; no options, privileges, filesystem directories, or system schema behavior |
| `DROP DATABASE` / `DROP SCHEMA` | 🟡 | Drops MyLite catalog schemas and descriptor-owned base tables with optional `IF EXISTS`; existing-schema drops reuse descriptor-driven base-table cleanup, and missing-schema `IF EXISTS` succeeds with a statement warning count but no stored `SHOW WARNINGS` row or `@@warning_count`; no privileges, implicit commit behavior, filesystem directory cleanup, `RESTRICT`/`CASCADE`, or non-base objects |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Renders current optionless MyLite schema descriptors with fixed MySQL 8.4.9 default charset/collation/encryption text; no `IF NOT EXISTS`, mutable schema options, privileges, system schemas, or `sql_quote_show_create` |
| `USE` | 🟡 | Selects existing MyLite catalog schemas, including schemas created through public SQL; limited `DATABASE()` / `SCHEMA()` scalar selects observe the selected schema, and limited `@@character_set_database` / `@@collation_database` reads expose fixed schema defaults |

[Back to compatibility overview](../../COMPATIBILITY.md)
