# SQL schemas

Database/schema selection and DDL compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER DATABASE` / `ALTER SCHEMA` | ❌ | Schema default options |
| `CREATE DATABASE` / `CREATE SCHEMA` | 🟡 | Creates persistent MyLite catalog schemas without `IF NOT EXISTS`, options, privileges, or system schema behavior |
| `DROP DATABASE` / `DROP SCHEMA` | 🟡 | Drops MyLite catalog schemas and descriptor-owned base tables; no `IF EXISTS`, privileges, implicit commit behavior, or non-base objects |
| `USE` | 🟡 | Selects existing MyLite catalog schemas, including schemas created through public SQL; `DATABASE()` remains unsupported |

[Back to compatibility overview](../../COMPATIBILITY.md)
