# SQL utility statements

General SQL utility and introspection statements.

| Statement | Status | Notes |
| --- | --- | --- |
| `DESCRIBE` / `DESC` | 🟡 | Limited table-column introspection alias for the supported `SHOW COLUMNS` subset; no column filters, wildcard patterns, execution-plan `EXPLAIN`, formats, or statement analysis |
| `EXPLAIN` | 🟡 | Limited table-column introspection alias for the supported `SHOW COLUMNS` subset; supports only `EXPLAIN table_name`, with no column filters, wildcard patterns, execution plans, `FORMAT`, `ANALYZE`, `FOR SCHEMA`, `FOR DATABASE`, or `FOR CONNECTION` |
| `HELP` | ❌ | Server help lookup result-set semantics |

[Back to compatibility overview](../../COMPATIBILITY.md)
