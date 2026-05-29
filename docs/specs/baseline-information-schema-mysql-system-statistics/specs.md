# Baseline INFORMATION_SCHEMA mysql System STATISTICS

This slice extends the supported `mysql` schema optimizer-statistics metadata
surface so `INFORMATION_SCHEMA.STATISTICS` exposes index rows for
`mysql.innodb_table_stats` and `mysql.innodb_index_stats`. The rows describe
the synthetic tables' primary keys and align with the already-supported
`SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS` output for the same tables.
The separate `baseline-mysql-component-table` slice extends the same metadata
path to the single `PRIMARY(component_id)` row for `mysql.component`.
The `baseline-mysql-func-table` slice extends it to the single `PRIMARY(name)`
row for `mysql.func`.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_information_schema_mysql_system_statistics_expectations.sh`.

The MySQL 8.4 manual defines `INFORMATION_SCHEMA.STATISTICS` as index metadata
with one row per key part. Runtime checks against MySQL 8.4.9 confirm that the
two InnoDB persistent-statistics tables each expose a visible `PRIMARY` BTREE
index.

## Supported Behavior

The following query surface is supported through the existing
`INFORMATION_SCHEMA` query engine:

```sql
SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats');
```

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior are inherited from the existing information-schema implementation.
No parser changes are required.

## Row Metadata

`INFORMATION_SCHEMA.STATISTICS` returns the following rows for
`mysql.innodb_table_stats`:

| TABLE_CATALOG | TABLE_SCHEMA | TABLE_NAME | NON_UNIQUE | INDEX_SCHEMA | INDEX_NAME | SEQ_IN_INDEX | COLUMN_NAME | COLLATION | CARDINALITY | SUB_PART | PACKED | NULLABLE | INDEX_TYPE | COMMENT | INDEX_COMMENT | IS_VISIBLE | EXPRESSION |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `def` | `mysql` | `innodb_table_stats` | `0` | `mysql` | `PRIMARY` | `1` | `database_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `def` | `mysql` | `innodb_table_stats` | `0` | `mysql` | `PRIMARY` | `2` | `table_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |

`INFORMATION_SCHEMA.STATISTICS` returns the following rows for
`mysql.innodb_index_stats`:

| TABLE_CATALOG | TABLE_SCHEMA | TABLE_NAME | NON_UNIQUE | INDEX_SCHEMA | INDEX_NAME | SEQ_IN_INDEX | COLUMN_NAME | COLLATION | CARDINALITY | SUB_PART | PACKED | NULLABLE | INDEX_TYPE | COMMENT | INDEX_COMMENT | IS_VISIBLE | EXPRESSION |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `def` | `mysql` | `innodb_index_stats` | `0` | `mysql` | `PRIMARY` | `1` | `database_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `def` | `mysql` | `innodb_index_stats` | `0` | `mysql` | `PRIMARY` | `2` | `table_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `def` | `mysql` | `innodb_index_stats` | `0` | `mysql` | `PRIMARY` | `3` | `index_name` | `A` | `2` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |
| `def` | `mysql` | `innodb_index_stats` | `0` | `mysql` | `PRIMARY` | `4` | `stat_name` | `A` | `6` | `NULL` | `NULL` | `` | `BTREE` | `` | `` | `YES` | `NULL` |

`CARDINALITY` values are deterministic MyLite-owned placeholders matching the
fresh MySQL 8.4.9 runtime evidence for these built-in statistics rows. They are
not live storage-engine estimates and do not change when MyLite adds
descriptor-owned statistics rows for user tables.

## Diagnostics And Limits

- Unsupported `mysql` system tables remain omitted from MyLite's
  `INFORMATION_SCHEMA.STATISTICS` rows. Built-in directory visibility through
  `SHOW TABLES`, `SHOW TABLE STATUS`, or `INFORMATION_SCHEMA.TABLES` does not
  imply complete system-table index catalogs.
- MyLite does not expose MySQL's broader `mysql` schema index metadata, grant
  table indexes, data-dictionary table indexes, privilege filtering, functional
  indexes, generated invisible primary-key system behavior, live
  storage-engine statistics, or `ANALYZE TABLE` statistics refresh semantics in
  this slice.
- The existing descriptor-backed `INFORMATION_SCHEMA.STATISTICS` rows for
  persistent user tables are unchanged.
- `mysql.component` primary-key metadata is specified and tested by
  `baseline-mysql-component-table`.
- `mysql.func` primary-key metadata is specified and tested by
  `baseline-mysql-func-table`.
- Writes to `mysql` system tables remain rejected by the existing built-in
  schema write-protection rules.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing information-schema `SELECT` support is
  reused.
- Analyzer/runtime: appends MyLite-owned rows while building
  `INFORMATION_SCHEMA.STATISTICS`.
- Catalog metadata: reuses MyLite-owned `mysql_system_table_definition` key
  markers for supported synthetic mysql tables.
- Storage/VFS/SQLite: unchanged. No physical `mysql` table, SQLite reflection,
  virtual table, or SQLite fork patch is introduced.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT TABLE_SCHEMA, TABLE_NAME, NON_UNIQUE, INDEX_SCHEMA, INDEX_NAME,
       SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART, PACKED,
       NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE, EXPRESSION
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
 ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, COLUMN_NAME, CARDINALITY
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME = 'innodb_index_stats'
   AND SEQ_IN_INDEX >= 3
 ORDER BY SEQ_IN_INDEX;
```

Observed output:

```text
8.4.9
mysql	innodb_index_stats	0	mysql	PRIMARY	1	database_name	A	2	NULL	NULL		BTREE			YES	NULL
mysql	innodb_index_stats	0	mysql	PRIMARY	2	table_name	A	2	NULL	NULL		BTREE			YES	NULL
mysql	innodb_index_stats	0	mysql	PRIMARY	3	index_name	A	2	NULL	NULL		BTREE			YES	NULL
mysql	innodb_index_stats	0	mysql	PRIMARY	4	stat_name	A	6	NULL	NULL		BTREE			YES	NULL
mysql	innodb_table_stats	0	mysql	PRIMARY	1	database_name	A	2	NULL	NULL		BTREE			YES	NULL
mysql	innodb_table_stats	0	mysql	PRIMARY	2	table_name	A	2	NULL	NULL		BTREE			YES	NULL
innodb_index_stats	index_name	2
innodb_index_stats	stat_name	6
```

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_mysql_system_statistics_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_mysql_system_statistics|mysql_func_table|mysql_component_table|mysql_system_show_index|mysql_system_show_columns|mysql_innodb_table_stats|mysql_innodb_index_stats)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_mysql_system_statistics_expectations.sh
git diff --check
cmake --workflow --preset check
```
