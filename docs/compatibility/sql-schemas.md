# SQL schemas

Database/schema selection and DDL compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER DATABASE` / `ALTER SCHEMA` | ❌ | Schema default options |
| `CREATE DATABASE` / `CREATE SCHEMA` | ❌ | Schema creation options |
| `DROP DATABASE` / `DROP SCHEMA` | ❌ | Schema removal semantics |
| `USE` | 🟡 | Selects an existing MyLite catalog schema for the limited table lifecycle subset; public schema creation/listing remains unsupported |

[Back to compatibility overview](../../COMPATIBILITY.md)
