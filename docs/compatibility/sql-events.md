# SQL events

Event Scheduler DDL and metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER EVENT` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no event scheduler metadata or body changes |
| `CREATE EVENT` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no event definition, scheduling, or metadata |
| `DROP EVENT` | ⚪ | Broad syntax is parsed and rejected at runtime with a stored-program unsupported diagnostic; no event metadata deletion |
| `INFORMATION_SCHEMA.EVENTS` | 🟡 | Queryable empty synthetic Event Scheduler catalog with MySQL 8.4.9-shaped columns and matching system metadata; no stored event descriptors, definitions, scheduling, definers, privileges, or execution |

[Back to compatibility overview](../../COMPATIBILITY.md)
