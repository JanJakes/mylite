# Baseline mysql System SHOW COLUMNS

This slice extends the supported `mysql` schema optimizer-statistics metadata
surface so `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW FIELDS`, `DESCRIBE`, and
`DESC` can introspect supported mysql system tables. The original slice covered
`mysql.innodb_table_stats` and `mysql.innodb_index_stats`; the separate
`baseline-mysql-component-table`, `baseline-mysql-func-table`,
`baseline-mysql-plugin-table`, `baseline-mysql-cost-tables`,
`baseline-mysql-servers-table`, `baseline-mysql-gtid-executed-table`, and
`baseline-mysql-log-tables`, and `baseline-mysql-time-zone-tables`
slices extend the same metadata path to `mysql.component`, `mysql.func`,
`mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`,
`mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, and the
`mysql.time_zone*` table family. The tables are limited read-only synthetic
system tables in MyLite; these features reuse owned column metadata rather
than adding physical system tables.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
- MySQL 8.4 Reference Manual, `Extensions to SHOW Statements`:
  <https://dev.mysql.com/doc/refman/8.4/en/extended-show.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_system_show_columns_expectations.sh`.

MySQL documents that `SHOW COLUMNS` reports column metadata for tables and
views, accepts `FULL`, `LIKE`, and `WHERE`, and that `DESCRIBE` provides a
similar table-column introspection surface. Runtime checks against MySQL 8.4.9
confirm that supported mysql system tables use ordinary `SHOW COLUMNS` result
shapes.

## Supported Behavior

The supported targets are:

```sql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.innodb_table_stats
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.innodb_index_stats
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.component
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.func
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.plugin
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.server_cost
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.engine_cost
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.servers
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.gtid_executed
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.general_log
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.slow_log
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.time_zone
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.time_zone_leap_second
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.time_zone_name
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.time_zone_transition
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} mysql.time_zone_transition_type
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} innodb_table_stats {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} innodb_index_stats {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} component {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} func {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} plugin {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} server_cost {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} engine_cost {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} servers {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} gtid_executed {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} general_log {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} slow_log {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} time_zone {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} time_zone_leap_second {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} time_zone_name {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} time_zone_transition {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} time_zone_transition_type {FROM | IN} mysql
DESCRIBE mysql.innodb_table_stats
DESCRIBE mysql.innodb_index_stats
DESCRIBE mysql.component
DESCRIBE mysql.func
DESCRIBE mysql.plugin
DESCRIBE mysql.server_cost
DESCRIBE mysql.engine_cost
DESCRIBE mysql.servers
DESCRIBE mysql.gtid_executed
DESCRIBE mysql.general_log
DESCRIBE mysql.slow_log
DESCRIBE mysql.time_zone
DESCRIBE mysql.time_zone_leap_second
DESCRIBE mysql.time_zone_name
DESCRIBE mysql.time_zone_transition
DESCRIBE mysql.time_zone_transition_type
DESC mysql.innodb_table_stats
DESC mysql.innodb_index_stats
DESC mysql.component
DESC mysql.func
DESC mysql.plugin
DESC mysql.server_cost
DESC mysql.engine_cost
DESC mysql.servers
DESC mysql.gtid_executed
DESC mysql.general_log
DESC mysql.slow_log
DESC mysql.time_zone
DESC mysql.time_zone_leap_second
DESC mysql.time_zone_name
DESC mysql.time_zone_transition
DESC mysql.time_zone_transition_type
```

Unqualified forms are supported after `USE mysql`:

```sql
USE mysql;
SHOW COLUMNS FROM innodb_table_stats;
SHOW FULL COLUMNS FROM innodb_index_stats;
SHOW COLUMNS FROM component;
SHOW COLUMNS FROM func;
SHOW COLUMNS FROM plugin;
SHOW COLUMNS FROM server_cost;
SHOW COLUMNS FROM engine_cost;
SHOW COLUMNS FROM servers;
SHOW COLUMNS FROM gtid_executed;
SHOW COLUMNS FROM general_log;
SHOW COLUMNS FROM slow_log;
SHOW COLUMNS FROM time_zone;
SHOW COLUMNS FROM time_zone_name;
DESCRIBE innodb_table_stats;
DESCRIBE plugin;
DESCRIBE server_cost;
DESCRIBE gtid_executed;
DESC servers;
```

`LIKE` filters and the existing limited `SHOW COLUMNS WHERE` evaluator apply to
the generated rows. `WHERE` predicates are evaluated against the displayed
output columns, with the same supported operators and literal restrictions as
descriptor-backed `SHOW COLUMNS`.

## Row Metadata

`SHOW COLUMNS FROM mysql.innodb_table_stats` returns six rows:

| Field | Type | Null | Key | Default | Extra |
| --- | --- | --- | --- | --- | --- |
| `database_name` | `varchar(64)` | `NO` | `PRI` | `NULL` | `` |
| `table_name` | `varchar(199)` | `NO` | `PRI` | `NULL` | `` |
| `last_update` | `timestamp` | `NO` | `` | `CURRENT_TIMESTAMP` | `DEFAULT_GENERATED on update CURRENT_TIMESTAMP` |
| `n_rows` | `bigint unsigned` | `NO` | `` | `NULL` | `` |
| `clustered_index_size` | `bigint unsigned` | `NO` | `` | `NULL` | `` |
| `sum_of_other_index_sizes` | `bigint unsigned` | `NO` | `` | `NULL` | `` |

`SHOW COLUMNS FROM mysql.innodb_index_stats` returns eight rows:

| Field | Type | Null | Key | Default | Extra |
| --- | --- | --- | --- | --- | --- |
| `database_name` | `varchar(64)` | `NO` | `PRI` | `NULL` | `` |
| `table_name` | `varchar(199)` | `NO` | `PRI` | `NULL` | `` |
| `index_name` | `varchar(64)` | `NO` | `PRI` | `NULL` | `` |
| `last_update` | `timestamp` | `NO` | `` | `CURRENT_TIMESTAMP` | `DEFAULT_GENERATED on update CURRENT_TIMESTAMP` |
| `stat_name` | `varchar(64)` | `NO` | `PRI` | `NULL` | `` |
| `stat_value` | `bigint unsigned` | `NO` | `` | `NULL` | `` |
| `sample_size` | `bigint unsigned` | `YES` | `` | `NULL` | `` |
| `stat_description` | `varchar(1024)` | `NO` | `` | `NULL` | `` |

`SHOW COLUMNS FROM mysql.component` returns three rows specified by
`baseline-mysql-component-table`: `component_id int unsigned NOT NULL
auto_increment PRIMARY KEY`, `component_group_id int unsigned NOT NULL`, and
`component_urn text NOT NULL`.

`SHOW COLUMNS FROM mysql.func` returns four rows specified by
`baseline-mysql-func-table`: `name char(64) NOT NULL DEFAULT '' PRIMARY KEY`,
`ret tinyint NOT NULL DEFAULT 0`, `dl char(128) NOT NULL DEFAULT ''`, and
`type enum('function','aggregate') NOT NULL`.

`SHOW COLUMNS FROM mysql.plugin` returns two rows specified by
`baseline-mysql-plugin-table`: `name varchar(64) NOT NULL DEFAULT '' PRIMARY
KEY` and `dl varchar(128) NOT NULL DEFAULT ''`.

`SHOW COLUMNS FROM mysql.server_cost` returns five rows specified by
`baseline-mysql-cost-tables`: `cost_name varchar(64) NOT NULL PRIMARY KEY`,
`cost_value float NULL`, `last_update timestamp NOT NULL DEFAULT
CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP`, `comment varchar(1024) NULL`,
and generated `default_value float`.

`SHOW COLUMNS FROM mysql.engine_cost` returns seven rows specified by
`baseline-mysql-cost-tables`: `engine_name varchar(64) NOT NULL PRIMARY KEY`,
`device_type int NOT NULL PRIMARY KEY`, `cost_name varchar(64) NOT NULL
PRIMARY KEY`, `cost_value float NULL`, `last_update timestamp NOT NULL DEFAULT
CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP`, `comment varchar(1024) NULL`,
and generated `default_value float`.

`SHOW COLUMNS FROM mysql.servers` returns nine rows specified by
`baseline-mysql-servers-table`: `Server_name char(64) NOT NULL DEFAULT ''
PRIMARY KEY`, `Host char(255) NOT NULL DEFAULT ''`, `Db char(64) NOT NULL
DEFAULT ''`, `Username char(64) NOT NULL DEFAULT ''`, `Password char(64) NOT
NULL DEFAULT ''`, `Port int NOT NULL DEFAULT 0`, `Socket char(64) NOT NULL
DEFAULT ''`, `Wrapper char(64) NOT NULL DEFAULT ''`, and `Owner char(64) NOT
NULL DEFAULT ''`.

`SHOW COLUMNS FROM mysql.gtid_executed` returns four rows specified by
`baseline-mysql-gtid-executed-table`: `source_uuid char(36) NOT NULL PRIMARY
KEY`, `interval_start bigint NOT NULL PRIMARY KEY`, `interval_end bigint NOT
NULL`, and `gtid_tag char(32) NOT NULL PRIMARY KEY`. `SHOW FULL COLUMNS`
reports `utf8mb4_0900_ai_ci` for the `char` columns, SQL `NULL` for the
`bigint` column collations, fixed privileges, and MySQL-observed comments.

`SHOW COLUMNS FROM mysql.general_log` returns six rows specified by
`baseline-mysql-log-tables`: `event_time timestamp(6) NOT NULL DEFAULT
CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6)`, `user_host mediumtext NOT
NULL`, `thread_id bigint unsigned NOT NULL`, `server_id int unsigned NOT
NULL`, `command_type varchar(64) NOT NULL`, and `argument mediumblob NOT NULL`.

`SHOW COLUMNS FROM mysql.slow_log` returns twelve rows specified by
`baseline-mysql-log-tables`: `start_time timestamp(6) NOT NULL DEFAULT
CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6)`, `user_host mediumtext NOT
NULL`, `query_time time(6) NOT NULL`, `lock_time time(6) NOT NULL`, `rows_sent
int NOT NULL`, `rows_examined int NOT NULL`, `db varchar(512) NOT NULL`,
`last_insert_id int NOT NULL`, `insert_id int NOT NULL`, `server_id int
unsigned NOT NULL`, `sql_text mediumblob NOT NULL`, and `thread_id bigint
unsigned NOT NULL`.

The `baseline-mysql-time-zone-tables` slice specifies `SHOW COLUMNS` metadata
for `mysql.time_zone`, `mysql.time_zone_leap_second`, `mysql.time_zone_name`,
`mysql.time_zone_transition`, and `mysql.time_zone_transition_type`, including
their MySQL-observed integer, `bigint`, `tinyint unsigned`, `char`, and
`enum('Y','N')` column types, primary-key markers, defaults, and
`auto_increment` metadata for `time_zone.Time_zone_id`.

`SHOW FULL COLUMNS` adds `Collation`, `Privileges`, and `Comment`. For the
original optimizer-statistics rows, runtime evidence shows `utf8mb3_bin` for
the nonbinary `varchar` columns, SQL `NULL` for numeric and timestamp column
collations, `select,insert,update,references` for privileges, and an empty
comment for every column. Later per-table slices specify their own fixed
comments where MySQL exposes them.

`Default` values are SQL `NULL` for nullable or no-explicit-default rows, except
for `last_update`, which reports `CURRENT_TIMESTAMP`. The `last_update` `Extra`
value includes both `DEFAULT_GENERATED` and `on update CURRENT_TIMESTAMP`.

## Diagnostics And Limits

- Unsupported `mysql` system tables remain unsupported for `SHOW COLUMNS`.
  Built-in directory rows exposed through `SHOW TABLES`,
  `SHOW TABLE STATUS`, or `INFORMATION_SCHEMA.TABLES` do not imply complete
  system-table column catalogs.
- Missing `mysql` table names that are not in MyLite's built-in directory use
  the ordinary table-not-found diagnostic.
- `EXTENDED`, hidden columns, privilege filtering, full MySQL data-dictionary
  tables, and complete `mysql` system-table column support remain out of scope.
- Parser behavior is unchanged. Existing `SHOW COLUMNS` and `DESCRIBE` AST
  forms are reused.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged.
- Analyzer/runtime: detects supported `mysql` system targets before ordinary
  descriptor-backed `SHOW COLUMNS` resolution.
- Catalog metadata: reuses MyLite-owned `mysql_system_table_definition` column,
  key, extra, privilege, and collation metadata.
- Storage/VFS/SQLite: unchanged. No physical `mysql` table, SQLite reflection,
  virtual table, or SQLite fork patch is introduced.

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_system_show_columns_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_system_show_columns|mysql_gtid_executed_table|mysql_servers_table|mysql_func_table|mysql_component_table|mysql_innodb_table_stats|mysql_innodb_index_stats|show_columns_introspection)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_system_show_columns_expectations.sh
git diff --check
cmake --workflow --preset check
```
