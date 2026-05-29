# Baseline INFORMATION_SCHEMA mysql System Constraints

This slice extends MyLite's limited `mysql` schema metadata so the supported
synthetic `mysql.innodb_table_stats` and `mysql.innodb_index_stats` tables
also appear in constraint-oriented `INFORMATION_SCHEMA` catalogs. The rows
describe the tables' `PRIMARY` constraints and match the previously supported
`SHOW COLUMNS`, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS` primary-key
metadata.
The separate `baseline-mysql-component-table` slice extends the same metadata
path to the single `PRIMARY(component_id)` constraint for `mysql.component`.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html>
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-extensions-table.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_information_schema_mysql_system_constraints_expectations.sh`.

The MySQL manual defines `TABLE_CONSTRAINTS` as table-level constraint
metadata, `KEY_COLUMN_USAGE` as one row per constrained key column, and
`TABLE_CONSTRAINTS_EXTENSIONS` as extension attributes for table constraints.
Runtime checks against MySQL 8.4.9 show that the two supported InnoDB
persistent-statistics tables each expose one enforced `PRIMARY KEY` constraint,
with key-column rows matching the primary-key column order and `NULL`
referenced-column fields.

## Supported Behavior

The following metadata surfaces are supported through the existing
`INFORMATION_SCHEMA` query engine:

```sql
SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats');

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats');

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
 WHERE CONSTRAINT_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats');
```

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior are inherited from the existing information-schema implementation.
No parser changes are required.

## Row Metadata

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` returns these rows:

| CONSTRAINT_CATALOG | CONSTRAINT_SCHEMA | CONSTRAINT_NAME | TABLE_SCHEMA | TABLE_NAME | CONSTRAINT_TYPE | ENFORCED |
| --- | --- | --- | --- | --- | --- | --- |
| `def` | `mysql` | `PRIMARY` | `mysql` | `innodb_index_stats` | `PRIMARY KEY` | `YES` |
| `def` | `mysql` | `PRIMARY` | `mysql` | `innodb_table_stats` | `PRIMARY KEY` | `YES` |

`INFORMATION_SCHEMA.KEY_COLUMN_USAGE` returns these rows:

| CONSTRAINT_CATALOG | CONSTRAINT_SCHEMA | CONSTRAINT_NAME | TABLE_CATALOG | TABLE_SCHEMA | TABLE_NAME | COLUMN_NAME | ORDINAL_POSITION | POSITION_IN_UNIQUE_CONSTRAINT | REFERENCED_TABLE_SCHEMA | REFERENCED_TABLE_NAME | REFERENCED_COLUMN_NAME |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `def` | `mysql` | `PRIMARY` | `def` | `mysql` | `innodb_index_stats` | `database_name` | `1` | `NULL` | `NULL` | `NULL` | `NULL` |
| `def` | `mysql` | `PRIMARY` | `def` | `mysql` | `innodb_index_stats` | `table_name` | `2` | `NULL` | `NULL` | `NULL` | `NULL` |
| `def` | `mysql` | `PRIMARY` | `def` | `mysql` | `innodb_index_stats` | `index_name` | `3` | `NULL` | `NULL` | `NULL` | `NULL` |
| `def` | `mysql` | `PRIMARY` | `def` | `mysql` | `innodb_index_stats` | `stat_name` | `4` | `NULL` | `NULL` | `NULL` | `NULL` |
| `def` | `mysql` | `PRIMARY` | `def` | `mysql` | `innodb_table_stats` | `database_name` | `1` | `NULL` | `NULL` | `NULL` | `NULL` |
| `def` | `mysql` | `PRIMARY` | `def` | `mysql` | `innodb_table_stats` | `table_name` | `2` | `NULL` | `NULL` | `NULL` | `NULL` |

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` returns these rows:

| CONSTRAINT_CATALOG | CONSTRAINT_SCHEMA | CONSTRAINT_NAME | TABLE_NAME | ENGINE_ATTRIBUTE | SECONDARY_ENGINE_ATTRIBUTE |
| --- | --- | --- | --- | --- | --- |
| `def` | `mysql` | `PRIMARY` | `innodb_index_stats` | `NULL` | `NULL` |
| `def` | `mysql` | `PRIMARY` | `innodb_table_stats` | `NULL` | `NULL` |

## Diagnostics And Limits

- Unsupported `mysql` system tables remain omitted from these three
  information-schema constraint catalogs. Built-in directory visibility does
  not imply complete system-table constraint metadata.
- MyLite does not expose MySQL's broader `mysql` schema constraint catalogs,
  grant table constraints, data-dictionary table constraints, privilege
  filtering, foreign-key metadata, check-constraint metadata, storage-engine
  attributes, or secondary-engine attributes for built-in system tables in
  this slice.
- Existing descriptor-backed rows for persistent user tables remain unchanged.
- `mysql.component` primary-key metadata is specified and tested by
  `baseline-mysql-component-table`.
- Writes to `mysql` system tables remain rejected by the existing built-in
  schema write-protection rules.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing information-schema `SELECT` support is
  reused.
- Analyzer/runtime: appends MyLite-owned rows while building
  `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `TABLE_CONSTRAINTS_EXTENSIONS`.
- Catalog metadata: reuses MyLite-owned `mysql_system_table_definition`
  primary-key markers for supported synthetic mysql tables.
- Storage/VFS/SQLite: unchanged. No physical `mysql` table, SQLite reflection,
  virtual table, or SQLite fork patch is introduced.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_SCHEMA,
       TABLE_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
 ORDER BY TABLE_NAME, CONSTRAINT_NAME;
SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_CATALOG,
       TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION,
       POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
       REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
  FROM information_schema.key_column_usage
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
 ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;
SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_NAME,
       ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE
  FROM information_schema.table_constraints_extensions
 WHERE CONSTRAINT_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
 ORDER BY TABLE_NAME, CONSTRAINT_NAME;
```

Observed output:

```text
8.4.9
def	mysql	PRIMARY	mysql	innodb_index_stats	PRIMARY KEY	YES
def	mysql	PRIMARY	mysql	innodb_table_stats	PRIMARY KEY	YES
def	mysql	PRIMARY	def	mysql	innodb_index_stats	database_name	1	NULL	NULL	NULL	NULL
def	mysql	PRIMARY	def	mysql	innodb_index_stats	table_name	2	NULL	NULL	NULL	NULL
def	mysql	PRIMARY	def	mysql	innodb_index_stats	index_name	3	NULL	NULL	NULL	NULL
def	mysql	PRIMARY	def	mysql	innodb_index_stats	stat_name	4	NULL	NULL	NULL	NULL
def	mysql	PRIMARY	def	mysql	innodb_table_stats	database_name	1	NULL	NULL	NULL	NULL
def	mysql	PRIMARY	def	mysql	innodb_table_stats	table_name	2	NULL	NULL	NULL	NULL
def	mysql	PRIMARY	innodb_index_stats	NULL	NULL
def	mysql	PRIMARY	innodb_table_stats	NULL	NULL
```

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_mysql_system_constraints_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_mysql_system_constraints|mysql_component_table|information_schema_mysql_system_statistics|mysql_system_show_index|mysql_system_show_columns|mysql_innodb_table_stats|mysql_innodb_index_stats)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_mysql_system_constraints_expectations.sh
git diff --check
cmake --workflow --preset check
```
