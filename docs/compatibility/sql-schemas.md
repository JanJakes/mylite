# SQL schemas

Database/schema selection and DDL compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER DATABASE` / `ALTER SCHEMA` | ❌ | Schema default options |
| `CREATE DATABASE` / `CREATE SCHEMA` | 🟡 | Creates persistent MyLite catalog schemas with optional `IF NOT EXISTS`; existing-schema `IF NOT EXISTS` succeeds as a no-op with stored `Note 1007`, affected rows `1`, and no catalog or SQLite schema mutation; creating `information_schema` is rejected with MySQL-compatible access denied diagnostics; no options, privileges, filesystem directories, or other system schema behavior |
| `DROP DATABASE` / `DROP SCHEMA` | 🟡 | Drops MyLite catalog schemas and descriptor-owned base tables with optional `IF EXISTS`; existing-schema drops reuse descriptor-driven base-table cleanup, missing-schema `IF EXISTS` succeeds with a statement warning count but no stored `SHOW WARNINGS` row or `@@warning_count`, dropping `information_schema` is rejected with MySQL-compatible access denied diagnostics, and limited user-transaction implicit commit follows the SQL transaction subset; no privileges, filesystem directory cleanup, `RESTRICT`/`CASCADE`, or non-base objects |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Renders current optionless MyLite schema descriptors with fixed MySQL 8.4.9 default charset/collation/encryption text; no `IF NOT EXISTS`, mutable schema options, privileges, system schemas, or `sql_quote_show_create` |
| `USE` | 🟡 | Selects existing MyLite catalog schemas, including schemas created through public SQL, and the synthetic `information_schema` schema without creating a catalog descriptor; limited `DATABASE()` / `SCHEMA()` scalar selects observe the selected schema, limited metadata `SELECT` forms can resolve unqualified `INFORMATION_SCHEMA` tables while it is selected, and limited `@@character_set_database` / `@@collation_database` reads expose fixed schema defaults |

[Back to compatibility overview](../../COMPATIBILITY.md)
