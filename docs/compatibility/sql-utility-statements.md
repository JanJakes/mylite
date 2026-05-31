# SQL utility statements

General SQL utility and introspection statements.

| Statement | Status | Notes |
| --- | --- | --- |
| `DESCRIBE` / `DESC` | 🟡 | Limited table-column introspection alias for the supported `SHOW COLUMNS` subset, including supported `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, the `mysql.time_zone*` table family, `mysql.innodb_table_stats`, `mysql.innodb_index_stats`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, and `sys.x$schema_table_statistics_with_buffer` system-object metadata; no column filters, wildcard patterns, execution-plan `EXPLAIN`, formats, or statement analysis |
| `EXPLAIN` | 🟡 | Limited table-column introspection alias for the supported `SHOW COLUMNS` subset; supports only `EXPLAIN table_name`, with no column filters, wildcard patterns, execution plans, `FORMAT`, `ANALYZE`, `FOR SCHEMA`, `FOR DATABASE`, or `FOR CONNECTION` |
| `HELP` | ❌ | Server help lookup result-set semantics |

[Back to compatibility overview](../../COMPATIBILITY.md)
