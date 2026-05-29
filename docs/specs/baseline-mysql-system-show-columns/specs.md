# Baseline mysql System SHOW COLUMNS

This slice extends the supported `mysql` schema optimizer-statistics metadata
surface so `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW FIELDS`, `DESCRIBE`, and
`DESC` can introspect supported mysql system tables. The original slice covered
`mysql.innodb_table_stats` and `mysql.innodb_index_stats`; the separate
`baseline-mysql-component-table` and `baseline-mysql-func-table` slices extend
the same metadata path to `mysql.component` and `mysql.func`. The tables are
limited read-only synthetic system tables in MyLite; these features reuse owned
column metadata rather than adding physical system tables.

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
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} innodb_table_stats {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} innodb_index_stats {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} component {FROM | IN} mysql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} func {FROM | IN} mysql
DESCRIBE mysql.innodb_table_stats
DESCRIBE mysql.innodb_index_stats
DESCRIBE mysql.component
DESCRIBE mysql.func
DESC mysql.innodb_table_stats
DESC mysql.innodb_index_stats
DESC mysql.component
DESC mysql.func
```

Unqualified forms are supported after `USE mysql`:

```sql
USE mysql;
SHOW COLUMNS FROM innodb_table_stats;
SHOW FULL COLUMNS FROM innodb_index_stats;
SHOW COLUMNS FROM component;
SHOW COLUMNS FROM func;
DESCRIBE innodb_table_stats;
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

`SHOW FULL COLUMNS` adds `Collation`, `Privileges`, and `Comment`. Runtime
evidence shows `utf8mb3_bin` for the nonbinary `varchar` columns, SQL `NULL` for
numeric and timestamp column collations, `select,insert,update,references` for
privileges, and an empty comment for every column.

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
ctest --preset dev -R '^libmylite\.runtime\.(mysql_system_show_columns|mysql_func_table|mysql_component_table|mysql_innodb_table_stats|mysql_innodb_index_stats|show_columns_introspection)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_system_show_columns_expectations.sh
git diff --check
cmake --workflow --preset check
```
