# Baseline mysql System SHOW INDEX

This slice extends the supported `mysql` schema optimizer-statistics metadata
surface so `SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS` can introspect
`mysql.innodb_table_stats` and `mysql.innodb_index_stats`. The separate
`baseline-mysql-component-table`, `baseline-mysql-func-table`,
`baseline-mysql-plugin-table`, `baseline-mysql-cost-tables`,
`baseline-mysql-servers-table`, `baseline-mysql-gtid-executed-table`, and
`baseline-mysql-log-tables`, and `baseline-mysql-time-zone-tables`
slices extend the same path to `mysql.component`, `mysql.func`,
`mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`,
`mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, and the
`mysql.time_zone*` table family. The tables remain read-only synthetic system
tables; these features expose their primary-key shape or MySQL-observed
no-index shape without creating physical system tables.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `Extensions to SHOW Statements`:
  <https://dev.mysql.com/doc/refman/8.4/en/extended-show.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_system_show_index_expectations.sh`.

MySQL documents `SHOW [EXTENDED] {INDEX | INDEXES | KEYS} {FROM | IN} tbl_name
[{FROM | IN} db_name] [WHERE expr]`, with output columns matching the ODBC-like
index metadata surface. Runtime checks against MySQL 8.4.9 confirm that both
InnoDB persistent-statistics tables expose one visible `PRIMARY` BTREE index.

## Supported Behavior

The supported targets are:

```sql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.innodb_table_stats
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.innodb_index_stats
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.component
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.func
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.plugin
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.server_cost
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.engine_cost
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.servers
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.gtid_executed
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.general_log
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.slow_log
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.time_zone
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.time_zone_leap_second
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.time_zone_name
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.time_zone_transition
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} mysql.time_zone_transition_type
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} innodb_table_stats {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} innodb_index_stats {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} component {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} func {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} plugin {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} server_cost {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} engine_cost {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} servers {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} gtid_executed {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} general_log {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} slow_log {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} time_zone {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} time_zone_leap_second {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} time_zone_name {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} time_zone_transition {FROM | IN} mysql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} time_zone_transition_type {FROM | IN} mysql
```

Unqualified forms are supported after `USE mysql`:

```sql
USE mysql;
SHOW INDEX FROM innodb_table_stats;
SHOW KEYS FROM innodb_index_stats;
SHOW INDEX FROM component;
SHOW INDEX FROM func;
SHOW INDEX FROM plugin;
SHOW INDEX FROM server_cost;
SHOW INDEX FROM engine_cost;
SHOW INDEX FROM servers;
SHOW INDEX FROM gtid_executed;
SHOW INDEX FROM general_log;
SHOW INDEX FROM slow_log;
SHOW INDEX FROM time_zone;
SHOW INDEX FROM time_zone_transition_type;
```

The existing limited `SHOW INDEX WHERE` evaluator applies to the generated
rows. Predicates are evaluated against the displayed output columns, with the
same supported operators and literal restrictions as descriptor-backed
`SHOW INDEX`.

## Row Metadata

`Cardinality` values in MySQL `SHOW INDEX` output are sampled statistics and
can change as the reused comparison runtime accumulates rows. The MySQL
expectation artifact verifies the stable row shape and filters while omitting
that field; MyLite runtime coverage verifies the deterministic placeholder
cardinality values configured by each system-table definition.

`SHOW INDEX FROM mysql.innodb_table_stats` returns two rows:

| Table | Non_unique | Key_name | Seq_in_index | Column_name | Collation | Cardinality | Sub_part | Packed | Null | Index_type | Comment | Index_comment | Visible | Expression |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `innodb_table_stats` | `0` | `PRIMARY` | `1` | `database_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `innodb_table_stats` | `0` | `PRIMARY` | `2` | `table_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |

`SHOW INDEX FROM mysql.innodb_index_stats` returns four rows:

| Table | Non_unique | Key_name | Seq_in_index | Column_name | Collation | Cardinality | Sub_part | Packed | Null | Index_type | Comment | Index_comment | Visible | Expression |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `innodb_index_stats` | `0` | `PRIMARY` | `1` | `database_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `innodb_index_stats` | `0` | `PRIMARY` | `2` | `table_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `innodb_index_stats` | `0` | `PRIMARY` | `3` | `index_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `innodb_index_stats` | `0` | `PRIMARY` | `4` | `stat_name` | `A` | `6` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |

`SHOW INDEX FROM mysql.component` returns the single `PRIMARY(component_id)`
row specified by `baseline-mysql-component-table`, with `Cardinality = 0`.

`SHOW INDEX FROM mysql.func` returns the single `PRIMARY(name)` row specified
by `baseline-mysql-func-table`, with `Cardinality = 0`.

`SHOW INDEX FROM mysql.plugin` returns the single `PRIMARY(name)` row specified
by `baseline-mysql-plugin-table`, with `Cardinality = 1`.

`SHOW INDEX FROM mysql.server_cost` returns the single `PRIMARY(cost_name)` row
specified by `baseline-mysql-cost-tables`, with `Cardinality = 6`.

`SHOW INDEX FROM mysql.engine_cost` returns
`PRIMARY(cost_name, engine_name, device_type)` rows specified by
`baseline-mysql-cost-tables`, with `Cardinality = 2`. The primary-key order
intentionally differs from the table's column order.

`SHOW INDEX FROM mysql.servers` returns the single `PRIMARY(Server_name)` row
specified by `baseline-mysql-servers-table`, with `Cardinality = 0`.

`SHOW INDEX FROM mysql.gtid_executed` returns
`PRIMARY(source_uuid, gtid_tag, interval_start)` rows specified by
`baseline-mysql-gtid-executed-table`, with `Cardinality = 0`. The primary-key
order intentionally differs from the table's column order.

`SHOW INDEX FROM mysql.general_log` and `SHOW INDEX FROM mysql.slow_log`
return zero rows with the ordinary MySQL 8.4.9 `SHOW INDEX` column labels, as
specified by `baseline-mysql-log-tables`.

`SHOW INDEX` for the `mysql.time_zone*` table family returns the primary-key
rows specified by `baseline-mysql-time-zone-tables`: single-column primary
keys for `time_zone`, `time_zone_leap_second`, and `time_zone_name`, and
two-part primary keys for `time_zone_transition` and
`time_zone_transition_type`, with MySQL-observed cardinality placeholders.

`Cardinality` values are deterministic MyLite-owned placeholders matching the
fresh MySQL 8.4.9 runtime evidence for the built-in statistics rows. They are
not live storage-engine estimates and do not change when MyLite adds descriptor
rows to the synthetic statistics tables.

## Diagnostics And Limits

- Unsupported `mysql` system tables remain unsupported for `SHOW INDEX`.
  Built-in directory rows exposed through `SHOW TABLES`,
  `SHOW TABLE STATUS`, or `INFORMATION_SCHEMA.TABLES` do not imply complete
  system-table index catalogs.
- Missing `mysql` table names that are not in MyLite's built-in directory use
  the ordinary table-not-found diagnostic.
- `EXTENDED`, privilege filtering, live storage-engine statistics, functional
  indexes, secondary indexes on these system tables, physical data-dictionary
  tables, and complete `mysql` system-table index support remain out of scope.
- Parser behavior is unchanged. Existing `SHOW INDEX` AST forms are reused.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged.
- Analyzer/runtime: detects supported `mysql` system targets before ordinary
  descriptor-backed `SHOW INDEX` resolution.
- Catalog metadata: reuses MyLite-owned `mysql_system_table_definition` key
  markers to render primary-key rows.
- Storage/VFS/SQLite: unchanged. No physical `mysql` table, SQLite reflection,
  virtual table, or SQLite fork patch is introduced.

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_system_show_index_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_system_show_index|mysql_gtid_executed_table|mysql_servers_table|mysql_func_table|mysql_component_table|mysql_system_show_columns|mysql_innodb_table_stats|mysql_innodb_index_stats|show_index_empty_introspection)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_system_show_index_expectations.sh
git diff --check
cmake --workflow --preset check
```
