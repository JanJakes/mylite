# Baseline Performance Schema optional table absence

## Scope

This baseline covers Performance Schema tables documented for optional MySQL
components, plugins, storage engines, or Enterprise-only surfaces that are not
present in the MySQL 8.4.9 target runtime used by MyLite's compatibility suite:

- `performance_schema.clone_progress`
- `performance_schema.clone_status`
- `performance_schema.component_scheduler_tasks`
- `performance_schema.firewall_group_allowlist`
- `performance_schema.firewall_groups`
- `performance_schema.firewall_membership`
- `performance_schema.ndb_sync_excluded_objects`
- `performance_schema.ndb_sync_pending_objects`
- `performance_schema.replication_group_communication_information`
- `performance_schema.replication_group_configuration_version`
- `performance_schema.replication_group_member_actions`
- `performance_schema.tp_connections`
- `performance_schema.tp_thread_group_state`
- `performance_schema.tp_thread_group_stats`
- `performance_schema.tp_thread_state`

MyLite should not invent placeholder tables for this target-runtime profile. The
compatible behavior is explicit absence from metadata plus MySQL-shaped
table-not-found diagnostics for direct table access and table introspection.

## Compatibility authority

The specification is based on the MySQL 8.4 Reference Manual Performance Schema
table descriptions and optional component table pages:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-descriptions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-firewall-tables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-tp-connections-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-replication-group-communication-information-table.html>

Runtime behavior was verified against a MySQL 8.4.9 container named
`mylite-mysql-849` with:

```sql
SELECT VERSION();
SELECT requested.table_name,
       IF(t.table_name IS NULL, 'ABSENT', 'PRESENT') AS status
  FROM (
        SELECT 'clone_progress' AS table_name
        UNION ALL SELECT 'clone_status'
        UNION ALL SELECT 'component_scheduler_tasks'
        UNION ALL SELECT 'firewall_group_allowlist'
        UNION ALL SELECT 'firewall_groups'
        UNION ALL SELECT 'firewall_membership'
        UNION ALL SELECT 'ndb_sync_excluded_objects'
        UNION ALL SELECT 'ndb_sync_pending_objects'
        UNION ALL SELECT 'replication_group_communication_information'
        UNION ALL SELECT 'replication_group_configuration_version'
        UNION ALL SELECT 'replication_group_member_actions'
        UNION ALL SELECT 'tp_connections'
        UNION ALL SELECT 'tp_thread_group_state'
        UNION ALL SELECT 'tp_thread_group_stats'
        UNION ALL SELECT 'tp_thread_state'
       ) AS requested
  LEFT JOIN information_schema.tables AS t
    ON t.table_schema = 'performance_schema'
   AND t.table_name = requested.table_name
 ORDER BY requested.table_name;
```

Observed MySQL 8.4.9 behavior:

- all covered names are absent from `INFORMATION_SCHEMA.TABLES`;
- `INFORMATION_SCHEMA.COLUMNS`, `STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, and `TABLE_CONSTRAINTS_EXTENSIONS` have no rows for the
  covered names;
- `SHOW FULL TABLES FROM performance_schema` and `SHOW TABLE STATUS FROM
  performance_schema` return no rows for the covered names;
- direct reads, `SHOW COLUMNS`, `DESCRIBE`, and `SHOW INDEX` return
  `1146 / 42S02` with `Table 'performance_schema.<name>' doesn't exist`.

## MyLite behavior

MyLite treats the covered names as absent built-in schema tables for the target
runtime profile. It must:

- omit them from built-in Performance Schema table descriptors;
- omit metadata rows from information_schema and SHOW metadata surfaces;
- return `1146 / 42S02` diagnostics for direct reads and table-specific
  introspection;
- resolve unqualified table names the same way after `USE performance_schema`.

No rows, indexes, constraints, or table status entries are synthesized.

## Explicit gaps

MyLite does not implement Clone, Enterprise Firewall, NDB synchronization, Group
Replication optional management, or Enterprise Thread Pool instrumentation. If a
future target profile enables one of these surfaces, that profile should add a
separate feature with MySQL-runtime-verified table shape, rows, metadata, and
diagnostics.

## Runtime and storage design

This slice intentionally adds no runtime table descriptors and no storage. It
only verifies the absence contract already expected from unsupported built-in
schema objects. No SQLite table, file-format change, dependency, public SQLite
extension API, or targeted SQLite fork hook is required.

No SQL grammar change is required. Existing metadata query parsing covers the
tested surfaces:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
show_tables_statement ::= SHOW full_opt TABLES from_schema_opt show_filter_opt.
show_table_status_statement ::= SHOW TABLE STATUS from_schema_opt show_filter_opt.
```

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_optional_absence_expectations.sh`
  verifies MySQL 8.4.9 absence from metadata and table-specific diagnostics.
- `packages/libmylite/tests/runtime_performance_schema_optional_absence_test.c`
  verifies MyLite matches the same absence behavior.
