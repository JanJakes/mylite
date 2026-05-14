# SQL triggers

Trigger DDL, ordering, body, definer, and metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `CREATE TRIGGER` | ❌ | Trigger timing, event, ordering, body, definer, and metadata |
| `DROP TRIGGER` | ❌ | Trigger deletion and metadata cleanup |
| `INFORMATION_SCHEMA.TRIGGERS` | 🟡 | Queryable empty synthetic trigger catalog with MySQL 8.4.9-shaped columns and matching system metadata; no trigger descriptors, definitions, definers, privileges, or execution |

[Back to compatibility overview](../../COMPATIBILITY.md)
