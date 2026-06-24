# SQL triggers

Trigger DDL, ordering, body, definer, and metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `CREATE TRIGGER` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no trigger descriptors, execution, timing/event metadata, definer handling, or persistence |
| `DROP TRIGGER` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no trigger deletion or metadata cleanup |
| `INFORMATION_SCHEMA.TRIGGERS` | 🟡 | Queryable synthetic trigger catalog with MySQL 8.4.9-shaped columns, metadata-only rows for the two built-in `sys.sys_config` triggers, empty user-trigger rows, and matching system metadata; no user trigger descriptors, definitions, definers, privileges, or execution |

[Back to compatibility overview](../../COMPATIBILITY.md)
