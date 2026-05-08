# SQL views

View DDL, metadata, security, and option compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER VIEW` | ❌ | View replacement semantics |
| `CREATE VIEW` | ❌ | View metadata and security |
| `DROP VIEW` | ❌ | Multi-view drop and warnings |
| CREATE VIEW options | ❌ | ALGORITHM, DEFINER, CHECK OPTION |
| Updatable views with LIMIT variable | 🟡 | Limited scalar `@@updatable_views_with_limit` reads report fixed enabled value `YES`; no mutable checking state, view DDL, view metadata, view DML, check options, or privileges |

[Back to compatibility overview](../../COMPATIBILITY.md)
