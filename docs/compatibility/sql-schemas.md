# SQL schemas

Database/schema selection and DDL compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER DATABASE` / `ALTER SCHEMA` | ❌ | Schema default options |
| `CREATE DATABASE` / `CREATE SCHEMA` | 🟡 | Creates persistent MyLite catalog schemas without `IF NOT EXISTS`, options, privileges, or system schema behavior |
| `DROP DATABASE` / `DROP SCHEMA` | 🟡 | Drops MyLite catalog schemas and descriptor-owned base tables; no `IF EXISTS`, privileges, implicit commit behavior, or non-base objects |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Renders current optionless MyLite schema descriptors with fixed MySQL 8.4.9 default charset/collation/encryption text; no `IF NOT EXISTS`, mutable schema options, privileges, system schemas, or `sql_quote_show_create` |
| `USE` | 🟡 | Selects existing MyLite catalog schemas, including schemas created through public SQL; limited `DATABASE()` / `SCHEMA()` scalar selects observe the selected schema |

[Back to compatibility overview](../../COMPATIBILITY.md)
