# Baseline mysql.gtid_executed Table

This slice makes `mysql.gtid_executed` a limited read-only synthetic system
table. MyLite already lists the table in the built-in `mysql` schema directory.
This feature adds direct empty reads plus MySQL-shaped metadata for the table's
columns, composite primary key, table status, and related information-schema
surfaces.

## Compatibility Authority

- MySQL 8.4 Reference Manual, GTID format and storage:
  <https://dev.mysql.com/doc/refman/8.4/en/replication-gtids-concepts.html>
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_gtid_executed_table_expectations.sh`.

The MySQL manual describes `mysql.gtid_executed` as the internal table used to
preserve assigned GTIDs that are not only present in the active binary log.
Runtime checks against the target MySQL 8.4.9 container show that the baseline
runtime has no `mysql.gtid_executed` rows, while ordinary column, primary-key,
and table-status introspection is visible.

## Supported Behavior

The supported direct-read forms are:

```sql
SELECT source_uuid, interval_start, interval_end, gtid_tag
  FROM mysql.gtid_executed;

USE mysql;
SELECT source_uuid, interval_start, interval_end, gtid_tag
  FROM gtid_executed;
```

The result has MySQL-shaped column labels and zero rows in MyLite's baseline
runtime. Explicit projection, unqualified resolution after `USE mysql`, and
`COUNT(*)` behavior are inherited from the existing MySQL-system-table query
engine. Direct `mysql.gtid_executed` reads do not add `ORDER BY` or `LIMIT`
syntax beyond the current MySQL-system-table read subset.

The supported metadata surfaces are:

```sql
SHOW COLUMNS FROM mysql.gtid_executed;
SHOW FULL COLUMNS FROM mysql.gtid_executed;
SHOW INDEX FROM mysql.gtid_executed;

SELECT ... FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';

SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';

SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'gtid_executed';

SHOW TABLE STATUS FROM mysql LIKE 'gtid_executed';
```

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
`gtid_executed` as a `BASE TABLE`; this slice does not change the built-in
table directory membership.

## Column Metadata

`mysql.gtid_executed` has four columns:

| Column | Type | Null | Key | Default | Extra | Collation | Comment |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `source_uuid` | `char(36)` | `NO` | `PRI` | `NULL` | `` | `utf8mb4_0900_ai_ci` | `uuid of the source where the transaction was originally executed.` |
| `interval_start` | `bigint` | `NO` | `PRI` | `NULL` | `` | `NULL` | `First number of interval.` |
| `interval_end` | `bigint` | `NO` | `` | `NULL` | `` | `NULL` | `Last number of interval.` |
| `gtid_tag` | `char(32)` | `NO` | `PRI` | `NULL` | `` | `utf8mb4_0900_ai_ci` | `GTID Tag.` |

`INFORMATION_SCHEMA.COLUMNS` uses these MySQL 8.4.9 values:

- `ORDINAL_POSITION` values are `1` through `4`;
- `source_uuid` uses `DATA_TYPE = 'char'`, `COLUMN_TYPE = 'char(36)'`,
  `CHARACTER_MAXIMUM_LENGTH = 36`, `CHARACTER_OCTET_LENGTH = 144`,
  `CHARACTER_SET_NAME = 'utf8mb4'`, and
  `COLLATION_NAME = 'utf8mb4_0900_ai_ci'`;
- `interval_start` and `interval_end` use `DATA_TYPE = 'bigint'`,
  `COLUMN_TYPE = 'bigint'`, `NUMERIC_PRECISION = 19`, and
  `NUMERIC_SCALE = 0`;
- `gtid_tag` uses `DATA_TYPE = 'char'`, `COLUMN_TYPE = 'char(32)'`,
  `CHARACTER_MAXIMUM_LENGTH = 32`, `CHARACTER_OCTET_LENGTH = 128`,
  `CHARACTER_SET_NAME = 'utf8mb4'`, and
  `COLLATION_NAME = 'utf8mb4_0900_ai_ci'`;
- `PRIVILEGES` is `select,insert,update,references`;
- `GENERATION_EXPRESSION` is an empty string.

## Key And Constraint Metadata

The only key is `PRIMARY(source_uuid, gtid_tag, interval_start)`. The key-part
order is intentionally not the same as column order.

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` expose three rows:

- `NON_UNIQUE = 0`;
- `INDEX_NAME` / `Key_name` is `PRIMARY`;
- `SEQ_IN_INDEX` values are `1`, `2`, and `3`;
- `COLUMN_NAME` values are `source_uuid`, `gtid_tag`, and `interval_start`;
- `COLLATION = 'A'`;
- `CARDINALITY = 0`;
- `SUB_PART`, `PACKED`, and `EXPRESSION` are SQL `NULL`;
- `NULLABLE`, `COMMENT`, and `INDEX_COMMENT` are empty strings;
- `INDEX_TYPE = 'BTREE'`;
- `IS_VISIBLE = 'YES'`.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes one enforced primary-key row,
and `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes three ordered primary-key
column rows with no referenced table or column values. Extension-attribute rows
for the primary key are included in `TABLE_CONSTRAINTS_EXTENSIONS` with `NULL`
engine attributes.

## Table Status

`INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose:

| Field | Value |
| --- | --- |
| `TABLE_TYPE` / row type | `BASE TABLE` |
| `ENGINE` | `InnoDB` |
| `VERSION` | `10` |
| `ROW_FORMAT` | `Dynamic` |
| `TABLE_ROWS` / `Rows` | `0` |
| `AVG_ROW_LENGTH` / `Avg_row_length` | `0` |
| `DATA_LENGTH` / `Data_length` | `16384` |
| `MAX_DATA_LENGTH` / `Max_data_length` | `0` |
| `INDEX_LENGTH` / `Index_length` | `0` |
| `DATA_FREE` / `Data_free` | `4194304` |
| `AUTO_INCREMENT` / `Auto_increment` | `NULL` |
| `TABLE_COLLATION` / `Collation` | `utf8mb4_0900_ai_ci` |
| `CREATE_OPTIONS` / `Create_options` | `row_format=DYNAMIC stats_persistent=0` |
| `TABLE_COMMENT` / `Comment` | `` |

`CREATE_TIME` is a non-`NULL` datetime string. `UPDATE_TIME`, `CHECK_TIME`, and
`CHECKSUM` are SQL `NULL`. MyLite renders `CREATE_TIME` from the current
statement timestamp for the synthetic row, matching the non-`NULL` shape
without introducing durable server-startup state.

## Diagnostics And Limits

- The baseline table is empty. MyLite does not implement binary logging, GTID
  mode, GTID persistence, GTID table compression, replication applier state,
  GTID SQL functions, or `RESET BINARY LOGS AND GTIDS` side effects.
- Writes to `mysql.gtid_executed` remain blocked by the built-in schema write
  guard before catalog mutation.
- `SHOW CREATE TABLE mysql.gtid_executed` remains out of scope for this slice.
- Privilege filtering is not implemented.
- No physical `mysql.gtid_executed` table, SQLite virtual table, or SQLite fork
  patch is introduced.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `SELECT`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA`, and `SHOW TABLE STATUS` forms are reused.
- Analyzer/runtime: resolves `mysql.gtid_executed` through the existing
  MySQL-system-table definition path and synthesizes zero direct rows plus
  metadata rows.
- Catalog metadata: unchanged. The table definition is static MyLite-owned
  system metadata and is not stored in user catalogs.
- Storage/VFS/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT COUNT(*) FROM mysql.gtid_executed;
SHOW COLUMNS FROM mysql.gtid_executed;
SHOW FULL COLUMNS FROM mysql.gtid_executed;
SHOW INDEX FROM mysql.gtid_executed;
SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
       COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
       GENERATION_EXPRESSION
  FROM information_schema.columns
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='gtid_executed'
 ORDER BY ORDINAL_POSITION;
SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='gtid_executed';
SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
       POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
       REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
  FROM information_schema.key_column_usage
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='gtid_executed'
 ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION;
SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE,
       SECONDARY_ENGINE_ATTRIBUTE
  FROM information_schema.table_constraints_extensions
 WHERE CONSTRAINT_SCHEMA='mysql' AND TABLE_NAME='gtid_executed';
SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY,
       SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT,
       IS_VISIBLE, EXPRESSION
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='gtid_executed'
 ORDER BY INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
       AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
       AUTO_INCREMENT IS NULL, CREATE_TIME IS NULL, UPDATE_TIME IS NULL,
       CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
       TABLE_COMMENT
  FROM information_schema.tables
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='gtid_executed';
SHOW TABLE STATUS FROM mysql LIKE 'gtid_executed';
```

Observed output:

In the text block, `<empty>` denotes an empty string field at the end of a row.

```text
8.4.9
0
source_uuid	char(36)	NO	PRI	NULL	<empty>
interval_start	bigint	NO	PRI	NULL	<empty>
interval_end	bigint	NO		NULL	<empty>
gtid_tag	char(32)	NO	PRI	NULL	<empty>
source_uuid	char(36)	utf8mb4_0900_ai_ci	NO	PRI	NULL		select,insert,update,references	uuid of the source where the transaction was originally executed.
interval_start	bigint	NULL	NO	PRI	NULL		select,insert,update,references	First number of interval.
interval_end	bigint	NULL	NO		NULL		select,insert,update,references	Last number of interval.
gtid_tag	char(32)	utf8mb4_0900_ai_ci	NO	PRI	NULL		select,insert,update,references	GTID Tag.
gtid_executed	0	PRIMARY	1	source_uuid	A	0	NULL	NULL		BTREE			YES	NULL
gtid_executed	0	PRIMARY	2	gtid_tag	A	0	NULL	NULL		BTREE			YES	NULL
gtid_executed	0	PRIMARY	3	interval_start	A	0	NULL	NULL		BTREE			YES	NULL
source_uuid	1	NULL	NO	char	36	144	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	char(36)	PRI		select,insert,update,references	uuid of the source where the transaction was originally executed.	<empty>
interval_start	2	NULL	NO	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	PRI		select,insert,update,references	First number of interval.	<empty>
interval_end	3	NULL	NO	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint			select,insert,update,references	Last number of interval.	<empty>
gtid_tag	4	NULL	NO	char	32	128	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	char(32)	PRI		select,insert,update,references	GTID Tag.	<empty>
PRIMARY	PRIMARY KEY	YES
PRIMARY	source_uuid	1	NULL	NULL	NULL	NULL
PRIMARY	gtid_tag	2	NULL	NULL	NULL	NULL
PRIMARY	interval_start	3	NULL	NULL	NULL	NULL
PRIMARY	gtid_executed	NULL	NULL
PRIMARY	1	source_uuid	A	0	NULL	NULL		BTREE			YES	NULL
PRIMARY	2	gtid_tag	A	0	NULL	NULL		BTREE			YES	NULL
PRIMARY	3	interval_start	A	0	NULL	NULL		BTREE			YES	NULL
gtid_executed	BASE TABLE	InnoDB	10	Dynamic	0	0	16384	0	0	4194304	1	0	1	1	utf8mb4_0900_ai_ci	1	row_format=DYNAMIC stats_persistent=0	<empty>
```

`SHOW TABLE STATUS FROM mysql LIKE 'gtid_executed'` returns the same stable
status fields, with a non-`NULL` `Create_time` datetime.

`SELECT * FROM mysql.gtid_executed ORDER BY source_uuid, gtid_tag,
interval_start` returns an empty result set with the four `mysql.gtid_executed`
columns.

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_gtid_executed_table_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_gtid_executed_table|builtin_schema_table_directory|mysql_system_show_columns|mysql_system_show_index|information_schema_mysql_system_constraints|information_schema_mysql_system_statistics|mysql_component_table|mysql_func_table|mysql_servers_table|mysql_system_stats_table_status)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_gtid_executed_table_expectations.sh
git diff --check
cmake --workflow --preset check
```
