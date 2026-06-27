# Baseline Performance Schema User Defined Functions Table

## Scope

This baseline covers the queryable metadata and default row surface for
`performance_schema.user_defined_functions`.

MyLite exposes MySQL 8.4.9-shaped read-only metadata and the default
component/plugin rows observed in the target MySQL runtime. The slice does not
implement loadable function installation, `CREATE FUNCTION` registration,
function unload state, dynamic usage counts, or callable behavior for the
listed server/plugin functions.

## Compatibility Sources

- MySQL 8.4 Reference Manual: Performance Schema `user_defined_functions`
  table
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-user-defined-functions-table.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`user_defined_functions` has five columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `UDF_NAME` | `varchar(64)` | `NO` | `PRI` | `NULL` |
| `UDF_RETURN_TYPE` | `varchar(20)` | `NO` | | `NULL` |
| `UDF_TYPE` | `varchar(20)` | `NO` | | `NULL` |
| `UDF_LIBRARY` | `varchar(1024)` | `YES` | | `NULL` |
| `UDF_USAGE_COUNT` | `bigint` | `YES` | | `NULL` |

The text columns use `utf8mb4_0900_ai_ci`. The table has a `HASH` primary key
on `UDF_NAME`. `SHOW TABLE STATUS` reports `ENGINE = PERFORMANCE_SCHEMA`,
`ROW_FORMAT = Dynamic`, `TABLE_ROWS = 16`, `AUTO_INCREMENT = NULL`, and
collation `utf8mb4_0900_ai_ci`.

The MySQL 8.4.9 target runtime returns sixteen default rows:

| UDF_NAME | UDF_RETURN_TYPE | UDF_TYPE | UDF_LIBRARY | UDF_USAGE_COUNT |
| --- | --- | --- | --- | --- |
| `asynchronous_connection_failover_add_managed` | `char` | `function` | `NULL` | `1` |
| `asynchronous_connection_failover_add_source` | `char` | `function` | `NULL` | `1` |
| `asynchronous_connection_failover_delete_managed` | `char` | `function` | `NULL` | `1` |
| `asynchronous_connection_failover_delete_source` | `char` | `function` | `NULL` | `1` |
| `asynchronous_connection_failover_reset` | `char` | `function` | `NULL` | `1` |
| `innodb_redo_log_archive_flush` | `integer` | `function` | `NULL` | `1` |
| `innodb_redo_log_archive_start` | `integer` | `function` | `NULL` | `1` |
| `innodb_redo_log_archive_stop` | `integer` | `function` | `NULL` | `1` |
| `innodb_redo_log_consumer_advance` | `integer` | `function` | `NULL` | `1` |
| `innodb_redo_log_consumer_register` | `integer` | `function` | `NULL` | `1` |
| `innodb_redo_log_consumer_unregister` | `integer` | `function` | `NULL` | `1` |
| `innodb_redo_log_sharp_checkpoint` | `integer` | `function` | `NULL` | `1` |
| `innodb_set_open_files_limit` | `integer` | `function` | `NULL` | `1` |
| `mysqlx_error` | `char` | `function` | `NULL` | `1` |
| `mysqlx_generate_document_id` | `char` | `function` | `NULL` | `1` |
| `mysqlx_get_prepared_statement_id` | `integer` | `function` | `NULL` | `1` |

## MyLite Behavior

MyLite exposes `performance_schema.user_defined_functions` through the built-in
system-table catalog:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work through the
  common MySQL-system-table query path.
- `USE performance_schema` resolves unqualified `user_defined_functions`
  references.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
  `INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` expose MySQL-shaped
  metadata.
- DDL and DML writes against `performance_schema` continue to return
  `1044 / 42000` access-denied diagnostics.

This slice intentionally does not implement loadable-function lifecycle
operations, `CREATE FUNCTION`, `DROP FUNCTION`, `mysql.func` synchronization,
dynamic usage counts, unloading-state rows, plugin/component installation, or
execution support for the listed MySQL component/plugin functions.

## Parser, Storage, And SQLite Integration

No grammar changes are required. The implementation is a MyLite
wrapper/catalog feature that appends static descriptor rows. It does not
require SQLite public extension APIs, virtual tables, or targeted SQLite fork
patches.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, primary-key
metadata, constraints, table status, information-schema table metadata, and
the default row set. The C runtime test verifies the same MyLite surfaces plus
`DESCRIBE`, selected-schema resolution, and write protection.
