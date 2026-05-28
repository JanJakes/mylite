# SQL views

View DDL, metadata, security, and option compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER VIEW` | ❌ | View replacement semantics |
| `CREATE VIEW` | 🟡 | Limited metadata-only `CREATE VIEW view AS SELECT ... FROM base_table` for a single persistent base-table source and direct column/wildcard projection; stores durable MyLite descriptors and metadata definitions, but does not create SQLite views or support execution through the view |
| `DROP VIEW` | 🟡 | Limited descriptor drop with multi-view target lists, `IF EXISTS` notes, MySQL-style base-table rejection, and no physical SQLite object cleanup because baseline views are metadata-only |
| `INFORMATION_SCHEMA.VIEWS` | 🟡 | Queryable synthetic view catalog with MySQL 8.4.9-shaped columns and rows for baseline view descriptors; definitions, fixed definer/security/check metadata, charset/collation, and deliberately non-updatable status are stored by MyLite, but there is no privilege filtering, check-option enforcement, or view execution |
| `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` | 🟡 | Queryable empty synthetic view-to-stored-function dependency catalog with MySQL 8.4.9-shaped columns; no stored routine descriptors or view-to-routine dependency analysis |
| CREATE VIEW options | ❌ | ALGORITHM, DEFINER, CHECK OPTION |
| Updatable views with LIMIT variable | 🟡 | Limited scalar `@@updatable_views_with_limit` reads report fixed enabled value `YES`; baseline view descriptors are queryable, but there is no mutable checking state, view DML, check-option enforcement, or privileges |

[Back to compatibility overview](../../COMPATIBILITY.md)
