# SQL views

View DDL, metadata, security, and option compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER VIEW` | ❌ | View replacement semantics |
| `CREATE VIEW` | ❌ | View metadata and security |
| `DROP VIEW` | ❌ | Multi-view drop and warnings |
| `INFORMATION_SCHEMA.VIEWS` | 🟡 | Queryable empty synthetic view catalog with MySQL 8.4.9-shaped columns and matching system metadata; no stored view descriptors, definitions, dependencies, privilege filtering, or view execution |
| CREATE VIEW options | ❌ | ALGORITHM, DEFINER, CHECK OPTION |
| Updatable views with LIMIT variable | 🟡 | Limited scalar `@@updatable_views_with_limit` reads report fixed enabled value `YES`; separate empty `INFORMATION_SCHEMA.VIEWS` metadata is queryable, but there is no mutable checking state, view DDL, stored view descriptors, view DML, check options, or privileges |

[Back to compatibility overview](../../COMPATIBILITY.md)
