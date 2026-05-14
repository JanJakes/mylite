# SQL events

Event Scheduler DDL and metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER EVENT` | ❌ | Event scheduler metadata and body changes |
| `CREATE EVENT` | ❌ | Event definition and scheduler metadata |
| `DROP EVENT` | ❌ | Event metadata deletion |
| `INFORMATION_SCHEMA.EVENTS` | 🟡 | Queryable empty synthetic Event Scheduler catalog with MySQL 8.4.9-shaped columns and matching system metadata; no stored event descriptors, definitions, scheduling, definers, privileges, or execution |

[Back to compatibility overview](../../COMPATIBILITY.md)
