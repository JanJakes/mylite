# Baseline Performance Schema replication placeholders

## Scope

This baseline covers the MySQL 8.4.9 Performance Schema replication tables that
are present in the target runtime without Group Replication optional tables:

- `performance_schema.replication_applier_configuration`
- `performance_schema.replication_applier_filters`
- `performance_schema.replication_applier_global_filters`
- `performance_schema.replication_applier_status`
- `performance_schema.replication_applier_status_by_coordinator`
- `performance_schema.replication_applier_status_by_worker`
- `performance_schema.replication_asynchronous_connection_failover`
- `performance_schema.replication_asynchronous_connection_failover_managed`
- `performance_schema.replication_connection_configuration`
- `performance_schema.replication_connection_status`
- `performance_schema.replication_group_member_stats`
- `performance_schema.replication_group_members`

The implemented surface is metadata-compatible and read-only. MyLite exposes
table descriptors, column metadata, HASH primary/secondary index metadata where
MySQL exposes it, table status metadata, empty selected rows, selected-schema
resolution, and built-in-schema write protection.

## Compatibility authority

The specification is based on the MySQL 8.4 Reference Manual page for
Performance Schema replication tables:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-replication-tables.html>

Runtime metadata was verified against a MySQL 8.4.9 container named
`mylite-mysql-849` with:

```sql
SELECT VERSION();
SELECT 'replication_applier_configuration', COUNT(*)
  FROM performance_schema.replication_applier_configuration
UNION ALL SELECT 'replication_applier_filters', COUNT(*)
  FROM performance_schema.replication_applier_filters
UNION ALL SELECT 'replication_applier_global_filters', COUNT(*)
  FROM performance_schema.replication_applier_global_filters
UNION ALL SELECT 'replication_applier_status', COUNT(*)
  FROM performance_schema.replication_applier_status
UNION ALL SELECT 'replication_applier_status_by_coordinator', COUNT(*)
  FROM performance_schema.replication_applier_status_by_coordinator
UNION ALL SELECT 'replication_applier_status_by_worker', COUNT(*)
  FROM performance_schema.replication_applier_status_by_worker
UNION ALL SELECT 'replication_asynchronous_connection_failover', COUNT(*)
  FROM performance_schema.replication_asynchronous_connection_failover
UNION ALL SELECT 'replication_asynchronous_connection_failover_managed', COUNT(*)
  FROM performance_schema.replication_asynchronous_connection_failover_managed
UNION ALL SELECT 'replication_connection_configuration', COUNT(*)
  FROM performance_schema.replication_connection_configuration
UNION ALL SELECT 'replication_connection_status', COUNT(*)
  FROM performance_schema.replication_connection_status
UNION ALL SELECT 'replication_group_member_stats', COUNT(*)
  FROM performance_schema.replication_group_member_stats
UNION ALL SELECT 'replication_group_members', COUNT(*)
  FROM performance_schema.replication_group_members;
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE,
       COLUMN_TYPE, COLUMN_KEY, COLLATION_NAME, CHARACTER_SET_NAME,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, COLUMN_DEFAULT, EXTRA, PRIVILEGES
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('replication_applier_configuration',
                      'replication_applier_filters',
                      'replication_applier_global_filters',
                      'replication_applier_status',
                      'replication_applier_status_by_coordinator',
                      'replication_applier_status_by_worker',
                      'replication_asynchronous_connection_failover',
                      'replication_asynchronous_connection_failover_managed',
                      'replication_connection_configuration',
                      'replication_connection_status',
                      'replication_group_member_stats',
                      'replication_group_members')
 ORDER BY TABLE_NAME, ORDINAL_POSITION;
SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
       INDEX_TYPE, IS_VISIBLE
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('replication_applier_configuration',
                      'replication_applier_status',
                      'replication_applier_status_by_coordinator',
                      'replication_applier_status_by_worker',
                      'replication_connection_configuration',
                      'replication_connection_status')
 ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('replication_applier_configuration',
                      'replication_applier_status',
                      'replication_applier_status_by_coordinator',
                      'replication_applier_status_by_worker',
                      'replication_connection_configuration',
                      'replication_connection_status')
 ORDER BY TABLE_NAME, CONSTRAINT_NAME;
```

Observed MySQL 8.4.9 behavior:

- All covered tables returned zero rows in the baseline container.
- The six channel-status/configuration tables expose HASH primary-key metadata;
  `replication_applier_status_by_coordinator`,
  `replication_applier_status_by_worker`, and `replication_connection_status`
  also expose nullable `THREAD_ID` HASH secondary indexes.
- `replication_applier_status`,
  `replication_asynchronous_connection_failover`, and
  `replication_group_members` use `ROW_FORMAT='Fixed'`; the remaining covered
  tables use `ROW_FORMAT='Dynamic'`.
- Optional Group Replication tables that were absent in the target runtime are
  not part of this baseline.

## MyLite behavior

MyLite exposes all covered tables as read-only synthetic Performance Schema
tables. Each table returns an empty result set. MyLite does not implement
replication sources, channels, filters, applier state, asynchronous connection
failover state, or group membership.

The placeholders let applications discover expected table shapes and distinguish
"no embedded replication state" from "table does not exist".

## Explicit gaps

- No replication channel, source, relay-log, GTID, filter, applier-worker,
  failover, or Group Replication membership state is stored.
- `CHANGE REPLICATION SOURCE TO`, `START REPLICA`, `STOP REPLICA`, and related
  server-side replication controls remain embedded no-op or diagnostic surfaces
  documented under SQL replication.
- Optional Performance Schema tables absent from the MySQL 8.4.9 target runtime
  remain unsupported by this baseline.
- `TRUNCATE TABLE` and other write-like operations remain blocked by MyLite's
  built-in schema write-protection policy rather than MySQL's per-table
  Performance Schema mutation rules.

## Runtime and storage design

This is descriptor-only metadata plus empty query dispatch. It uses MyLite's
existing built-in table descriptor framework. No SQLite table, file format
change, public SQLite extension API, targeted SQLite fork hook, or new
dependency is required.

No new SQL grammar is required. Existing metadata query parsing covers:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
qualified_table_name ::= ident DOT ident.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
show_table_status_statement ::= SHOW TABLE STATUS from_schema_opt show_filter_opt.
```

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_replication_placeholders_expectations.sh`
  verifies MySQL 8.4.9 row counts, column, index, constraint, and table
  metadata.
- `packages/libmylite/tests/runtime_performance_schema_replication_placeholders_test.c`
  verifies MyLite empty query results, metadata surfaces, selected-schema
  resolution, and write protection.
