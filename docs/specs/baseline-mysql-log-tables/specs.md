# Baseline mysql Log Tables

This slice makes `mysql.general_log` and `mysql.slow_log` limited read-only
synthetic system tables. MyLite already lists both names in the built-in
`mysql` schema directory. This feature adds direct empty reads plus
MySQL-shaped column metadata, no-index/no-constraint metadata, CSV table status,
and related information-schema rows.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, MySQL server logs:
  <https://dev.mysql.com/doc/refman/8.4/en/server-logs.html>
- MySQL 8.4 Reference Manual, log output destinations:
  <https://dev.mysql.com/doc/refman/8.4/en/log-destinations.html>
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_log_tables_expectations.sh`.

The MySQL manual identifies `mysql.general_log` and `mysql.slow_log` as the log
system tables and states that default log tables use the `CSV` storage engine.
Runtime checks against the target MySQL 8.4.9 container show that the baseline
runtime has empty direct result sets for both tables, while ordinary column and
table-status introspection remains visible.

## Supported Behavior

The supported direct-read forms are:

```sql
SELECT event_time, user_host, thread_id, server_id, command_type, argument
  FROM mysql.general_log;

SELECT start_time, user_host, query_time, lock_time, rows_sent, rows_examined,
       db, last_insert_id, insert_id, server_id, sql_text, thread_id
  FROM mysql.slow_log;

USE mysql;
SELECT COUNT(*) FROM general_log;
SELECT COUNT(*) FROM slow_log;
```

The direct result sets have MySQL-shaped column labels and zero rows in
MyLite's baseline runtime. Explicit projection, unqualified resolution after
`USE mysql`, and `COUNT(*)` behavior are inherited from the existing
MySQL-system-table query engine. This slice does not add log-table `ORDER BY`,
`LIMIT`, or predicate forms beyond the current MySQL-system-table read subset.

The supported metadata surfaces are:

```sql
SHOW COLUMNS FROM mysql.general_log;
SHOW FULL COLUMNS FROM mysql.general_log;
SHOW INDEX FROM mysql.general_log;

SHOW COLUMNS FROM mysql.slow_log;
SHOW FULL COLUMNS FROM mysql.slow_log;
SHOW INDEX FROM mysql.slow_log;

SELECT ... FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('general_log', 'slow_log');

SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('general_log', 'slow_log');

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('general_log', 'slow_log');

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('general_log', 'slow_log');

SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('general_log', 'slow_log');

SHOW TABLE STATUS FROM mysql LIKE 'general_log';
SHOW TABLE STATUS FROM mysql LIKE 'slow_log';
```

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
both tables as `BASE TABLE`; this slice does not change directory membership.

## Column Metadata

`mysql.general_log` has six columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `event_time` | `timestamp(6)` | `NO` | `` | `CURRENT_TIMESTAMP(6)` | `DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)` | `NULL` |
| `user_host` | `mediumtext` | `NO` | `` | `NULL` | `` | `utf8mb3_general_ci` |
| `thread_id` | `bigint unsigned` | `NO` | `` | `NULL` | `` | `NULL` |
| `server_id` | `int unsigned` | `NO` | `` | `NULL` | `` | `NULL` |
| `command_type` | `varchar(64)` | `NO` | `` | `NULL` | `` | `utf8mb3_general_ci` |
| `argument` | `mediumblob` | `NO` | `` | `NULL` | `` | `NULL` |

`INFORMATION_SCHEMA.COLUMNS` uses these MySQL 8.4.9 values:

- `event_time` uses `DATA_TYPE = 'timestamp'`, `COLUMN_TYPE =
  'timestamp(6)'`, `DATETIME_PRECISION = 6`, and the generated default/on-update
  text above.
- `user_host` uses `DATA_TYPE = 'mediumtext'`, `COLUMN_TYPE = 'mediumtext'`,
  character maximum and octet lengths of `16777215`, character set `utf8mb3`,
  and collation `utf8mb3_general_ci`.
- `thread_id` uses `DATA_TYPE = 'bigint'`, `COLUMN_TYPE = 'bigint unsigned'`,
  `NUMERIC_PRECISION = 20`, and `NUMERIC_SCALE = 0`.
- `server_id` uses `DATA_TYPE = 'int'`, `COLUMN_TYPE = 'int unsigned'`,
  `NUMERIC_PRECISION = 10`, and `NUMERIC_SCALE = 0`.
- `command_type` uses `DATA_TYPE = 'varchar'`, `COLUMN_TYPE = 'varchar(64)'`,
  `CHARACTER_MAXIMUM_LENGTH = 64`, `CHARACTER_OCTET_LENGTH = 192`, character
  set `utf8mb3`, and collation `utf8mb3_general_ci`.
- `argument` uses `DATA_TYPE = 'mediumblob'`, `COLUMN_TYPE = 'mediumblob'`,
  and character maximum and octet lengths of `16777215`, with SQL `NULL`
  character set and collation names.

`mysql.slow_log` has twelve columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `start_time` | `timestamp(6)` | `NO` | `` | `CURRENT_TIMESTAMP(6)` | `DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)` | `NULL` |
| `user_host` | `mediumtext` | `NO` | `` | `NULL` | `` | `utf8mb3_general_ci` |
| `query_time` | `time(6)` | `NO` | `` | `NULL` | `` | `NULL` |
| `lock_time` | `time(6)` | `NO` | `` | `NULL` | `` | `NULL` |
| `rows_sent` | `int` | `NO` | `` | `NULL` | `` | `NULL` |
| `rows_examined` | `int` | `NO` | `` | `NULL` | `` | `NULL` |
| `db` | `varchar(512)` | `NO` | `` | `NULL` | `` | `utf8mb3_general_ci` |
| `last_insert_id` | `int` | `NO` | `` | `NULL` | `` | `NULL` |
| `insert_id` | `int` | `NO` | `` | `NULL` | `` | `NULL` |
| `server_id` | `int unsigned` | `NO` | `` | `NULL` | `` | `NULL` |
| `sql_text` | `mediumblob` | `NO` | `` | `NULL` | `` | `NULL` |
| `thread_id` | `bigint unsigned` | `NO` | `` | `NULL` | `` | `NULL` |

`INFORMATION_SCHEMA.COLUMNS` uses these MySQL 8.4.9 values:

- `start_time` matches the `general_log.event_time` timestamp metadata.
- `query_time` and `lock_time` use `DATA_TYPE = 'time'`, `COLUMN_TYPE =
  'time(6)'`, and `DATETIME_PRECISION = 6`.
- `rows_sent`, `rows_examined`, `last_insert_id`, and `insert_id` use
  `DATA_TYPE = 'int'`, `COLUMN_TYPE = 'int'`, `NUMERIC_PRECISION = 10`, and
  `NUMERIC_SCALE = 0`.
- `db` uses `DATA_TYPE = 'varchar'`, `COLUMN_TYPE = 'varchar(512)'`,
  `CHARACTER_MAXIMUM_LENGTH = 512`, `CHARACTER_OCTET_LENGTH = 1536`, character
  set `utf8mb3`, and collation `utf8mb3_general_ci`.
- `server_id`, `sql_text`, `thread_id`, and `user_host` use the same metadata
  as the corresponding `general_log` column families.

For every log-table column, `PRIVILEGES` is
`select,insert,update,references`, `COLUMN_COMMENT` is an empty string, and
`GENERATION_EXPRESSION` is an empty string.

## Key And Constraint Metadata

MySQL 8.4.9 exposes no indexes for either log table. MyLite therefore returns
zero rows for:

- `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`;
- `INFORMATION_SCHEMA.STATISTICS`;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`;
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`.

The absence of rows is intentional and differs from the primary-key metadata
used for supported InnoDB-backed `mysql` system tables.

## Table Status

`INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose:

| Field | `general_log` | `slow_log` |
| --- | --- | --- |
| `TABLE_TYPE` / row type | `BASE TABLE` | `BASE TABLE` |
| `ENGINE` | `CSV` | `CSV` |
| `VERSION` | `10` | `10` |
| `ROW_FORMAT` | `Dynamic` | `Dynamic` |
| `TABLE_ROWS` / `Rows` | `2` | `2` |
| `AVG_ROW_LENGTH` / `Avg_row_length` | `0` | `0` |
| `DATA_LENGTH` / `Data_length` | `0` | `0` |
| `MAX_DATA_LENGTH` / `Max_data_length` | `0` | `0` |
| `INDEX_LENGTH` / `Index_length` | `0` | `0` |
| `DATA_FREE` / `Data_free` | `0` | `0` |
| `AUTO_INCREMENT` / `Auto_increment` | `NULL` | `NULL` |
| `TABLE_COLLATION` / `Collation` | `utf8mb3_general_ci` | `utf8mb3_general_ci` |
| `CREATE_OPTIONS` / `Create_options` | `` | `` |
| `TABLE_COMMENT` / `Comment` | `General log` | `Slow log` |

`CREATE_TIME` is a non-`NULL` datetime string. `UPDATE_TIME`, `CHECK_TIME`, and
`CHECKSUM` are SQL `NULL`. MyLite renders `CREATE_TIME` from the current
statement timestamp for synthetic rows, matching the non-`NULL` shape without
introducing durable server-startup state. The row and storage-size fields are
runtime CSV table estimates on MySQL and can change with log state; the MySQL
expectation artifact verifies their numeric shape rather than fixed values.
The direct synthetic tables are empty.

## Diagnostics And Limits

- The baseline tables are empty. MyLite does not implement general-query-log
  or slow-query-log storage, log files, log-output variables, runtime log
  writing, log rotation, or log flushing.
- Writes to `mysql.general_log` and `mysql.slow_log` remain blocked by the
  built-in schema write guard before catalog mutation.
- `CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`, `RENAME TABLE`, `TRUNCATE
  TABLE`, `CHECK TABLE`, `LOCK TABLES`, and server-internal write behavior for
  log tables remain out of scope beyond existing built-in-schema diagnostics.
- `SHOW CREATE TABLE mysql.general_log` and `SHOW CREATE TABLE mysql.slow_log`
  remain out of scope for this slice.
- Privilege filtering is not implemented.
- No physical `mysql` log table, CSV file, SQLite virtual table, or SQLite fork
  patch is introduced.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `SELECT`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA`, and `SHOW TABLE STATUS` forms are reused.
- Analyzer/runtime: resolves the log tables through the existing
  MySQL-system-table definition path and synthesizes zero direct rows plus
  metadata rows.
- Catalog metadata: unchanged. The definitions are static MyLite-owned system
  metadata and are not stored in user catalogs.
- Storage/VFS/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SHOW FULL COLUMNS FROM mysql.general_log;
SHOW INDEX FROM mysql.general_log;
SELECT COUNT(*) FROM mysql.general_log;
SHOW TABLE STATUS FROM mysql LIKE 'general_log';
SHOW FULL COLUMNS FROM mysql.slow_log;
SHOW INDEX FROM mysql.slow_log;
SELECT COUNT(*) FROM mysql.slow_log;
SHOW TABLE STATUS FROM mysql LIKE 'slow_log';
SELECT TABLE_NAME, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH,
       DATA_LENGTH, INDEX_LENGTH, DATA_FREE, CREATE_OPTIONS, TABLE_COMMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME IN ('general_log','slow_log')
 ORDER BY TABLE_NAME;
```

Observed output, summarized with empty trailing strings rendered as `<empty>`:

```text
8.4.9
general_log columns: event_time timestamp(6), user_host mediumtext,
  thread_id bigint unsigned, server_id int unsigned,
  command_type varchar(64), argument mediumblob
general_log SHOW INDEX: zero rows
general_log COUNT(*): 0
general_log status: CSV, version 10, Dynamic, Rows 2, data/index/free 0,
  collation utf8mb3_general_ci, create options <empty>, comment General log
slow_log columns: start_time timestamp(6), user_host mediumtext,
  query_time time(6), lock_time time(6), rows_sent int, rows_examined int,
  db varchar(512), last_insert_id int, insert_id int, server_id int unsigned,
  sql_text mediumblob, thread_id bigint unsigned
slow_log SHOW INDEX: zero rows
slow_log COUNT(*): 0
slow_log status: CSV, version 10, Dynamic, Rows 2, data/index/free 0,
  collation utf8mb3_general_ci, create options <empty>, comment Slow log
INFORMATION_SCHEMA.TABLES rows:
  general_log CSV 10 Dynamic 2 0 0 0 0 <empty> General log
  slow_log CSV 10 Dynamic 2 0 0 0 0 <empty> Slow log
```

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_log_tables_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_log_tables|mysql_system_show_columns|mysql_system_show_index|mysql_gtid_executed_table|mysql_servers_table|mysql_func_table|mysql_component_table|information_schema_mysql_system_statistics|information_schema_mysql_system_constraints)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_log_tables_expectations.sh
git diff --check
cmake --workflow --preset check
```
