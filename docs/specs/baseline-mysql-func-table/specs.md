# Baseline mysql.func Table

This slice makes `mysql.func` a limited read-only synthetic system table.
MyLite already lists the table in the built-in `mysql` schema directory. This
feature adds direct empty reads plus MySQL-shaped metadata for the table's
columns, primary key, table status, and related information-schema surfaces.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, loadable-function `CREATE FUNCTION`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-function-loadable.html>
- MySQL 8.4 Reference Manual, loadable-function information:
  <https://dev.mysql.com/doc/refman/8.4/en/obtaining-loadable-function-information.html>
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
  `packages/libmylite/tests/mysql_baseline_mysql_func_table_expectations.sh`.

The MySQL manual identifies `mysql.func` as the registry for loadable functions
installed with loadable-function `CREATE FUNCTION`. Runtime checks against the
target MySQL 8.4.9 container show that the baseline runtime has no
loadable-function rows, while ordinary column, primary-key, and table-status
introspection is visible.

## Supported Behavior

The supported direct-read forms are:

```sql
SELECT name, ret, dl, type FROM mysql.func;

USE mysql;
SELECT name, ret, dl, type FROM func;
```

The result has MySQL-shaped column labels and zero rows in MyLite's baseline
runtime. Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and
`LIMIT` behavior are inherited from the existing MySQL-system-table query
engine.

The supported metadata surfaces are:

```sql
SHOW COLUMNS FROM mysql.func;
SHOW FULL COLUMNS FROM mysql.func;
SHOW INDEX FROM mysql.func;

SELECT ... FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'func';

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'func';

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'func';

SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'func';

SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'func';

SHOW TABLE STATUS FROM mysql LIKE 'func';
```

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
`func` as a `BASE TABLE`; this slice does not change the built-in table
directory membership.

## Column Metadata

`mysql.func` has four columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `name` | `char(64)` | `NO` | `PRI` | `` | `` | `utf8mb3_bin` |
| `ret` | `tinyint` | `NO` | `` | `0` | `` | `NULL` |
| `dl` | `char(128)` | `NO` | `` | `` | `` | `utf8mb3_bin` |
| `type` | `enum('function','aggregate')` | `NO` | `` | `NULL` | `` | `utf8mb3_general_ci` |

`INFORMATION_SCHEMA.COLUMNS` uses these MySQL 8.4.9 values:

- `ORDINAL_POSITION` values are `1`, `2`, `3`, and `4`;
- `name` has `DATA_TYPE = 'char'`, `COLUMN_TYPE = 'char(64)'`,
  `CHARACTER_MAXIMUM_LENGTH = 64`, `CHARACTER_OCTET_LENGTH = 192`,
  `CHARACTER_SET_NAME = 'utf8mb3'`, and `COLLATION_NAME = 'utf8mb3_bin'`;
- `ret` has `DATA_TYPE = 'tinyint'`, `COLUMN_TYPE = 'tinyint'`,
  `NUMERIC_PRECISION = 3`, and `NUMERIC_SCALE = 0`;
- `dl` has `DATA_TYPE = 'char'`, `COLUMN_TYPE = 'char(128)'`,
  `CHARACTER_MAXIMUM_LENGTH = 128`, `CHARACTER_OCTET_LENGTH = 384`,
  `CHARACTER_SET_NAME = 'utf8mb3'`, and `COLLATION_NAME = 'utf8mb3_bin'`;
- `type` has `DATA_TYPE = 'enum'`,
  `COLUMN_TYPE = "enum('function','aggregate')"`,
  `CHARACTER_MAXIMUM_LENGTH = 9`, `CHARACTER_OCTET_LENGTH = 27`,
  `CHARACTER_SET_NAME = 'utf8mb3'`, and
  `COLLATION_NAME = 'utf8mb3_general_ci'`;
- `PRIVILEGES` is `select,insert,update,references`;
- `COLUMN_COMMENT` and `GENERATION_EXPRESSION` are empty strings.

## Key And Constraint Metadata

The only key is `PRIMARY(name)`.

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` expose one row:

- `NON_UNIQUE = 0`;
- `INDEX_NAME` / `Key_name` is `PRIMARY`;
- `SEQ_IN_INDEX = 1`;
- `COLUMN_NAME = 'name'`;
- `COLLATION = 'A'`;
- `CARDINALITY = 0`;
- `SUB_PART`, `PACKED`, and `EXPRESSION` are SQL `NULL`;
- `NULLABLE`, `COMMENT`, and `INDEX_COMMENT` are empty strings;
- `INDEX_TYPE = 'BTREE'`;
- `IS_VISIBLE = 'YES'`.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes one enforced primary-key row,
and `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes one ordered primary-key
column row with no referenced table or column values. Extension-attribute rows
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
| `TABLE_COLLATION` / `Collation` | `utf8mb3_bin` |
| `CREATE_OPTIONS` / `Create_options` | `row_format=DYNAMIC stats_persistent=0` |
| `TABLE_COMMENT` / `Comment` | `User defined functions` |

`CREATE_TIME` is a non-`NULL` datetime string. `UPDATE_TIME`, `CHECK_TIME`, and
`CHECKSUM` are SQL `NULL`. MyLite renders `CREATE_TIME` from the current
statement timestamp for the synthetic row, matching the non-`NULL` shape
without introducing durable server-startup state.

## Diagnostics And Limits

- The baseline table is empty. MyLite does not implement loadable-function
  `CREATE FUNCTION`, loadable-function `DROP FUNCTION`, function loading,
  persisted `mysql.func` rows, function invocation, or startup loading.
- Writes to `mysql.func` remain blocked by the built-in schema write guard
  before catalog mutation.
- Other object-information system tables are governed by their own specs; this
  slice does not add plugin metadata.
- `SHOW CREATE TABLE mysql.func` remains out of scope for this slice.
- Privilege filtering is not implemented.
- No physical `mysql.func` table, SQLite virtual table, or SQLite fork patch is
  introduced.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `SELECT`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA`, and `SHOW TABLE STATUS` forms are reused.
- Analyzer/runtime: resolves `mysql.func` through the existing
  MySQL-system-table definition path and synthesizes zero direct rows plus
  metadata rows.
- Catalog metadata: unchanged. The table definition is static MyLite-owned
  system metadata and is not stored in user catalogs.
- Storage/VFS/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT COUNT(*) FROM mysql.func;
SHOW COLUMNS FROM mysql.func;
SHOW FULL COLUMNS FROM mysql.func;
SHOW INDEX FROM mysql.func;
SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
       COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
       GENERATION_EXPRESSION
  FROM information_schema.columns
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='func'
 ORDER BY ORDINAL_POSITION;
SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='func';
SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
       POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
       REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
  FROM information_schema.key_column_usage
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='func';
SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE,
       SECONDARY_ENGINE_ATTRIBUTE
  FROM information_schema.table_constraints_extensions
 WHERE CONSTRAINT_SCHEMA='mysql' AND TABLE_NAME='func';
SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART,
       PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
       EXPRESSION
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='func';
SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
       AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
       AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
       CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
       TABLE_COMMENT
  FROM information_schema.tables
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='func';
SHOW TABLE STATUS FROM mysql LIKE 'func';
SELECT ROW_COUNT();
```

Observed output:

```text
8.4.9
0
name	char(64)	NO	PRI	<empty Default>	<empty Extra>
ret	tinyint	NO		0	<empty Extra>
dl	char(128)	NO		<empty Default>	<empty Extra>
type	enum('function','aggregate')	NO		NULL	<empty Extra>
name	char(64)	utf8mb3_bin	NO	PRI	<empty Default>		select,insert,update,references	<empty Comment>
ret	tinyint	NULL	NO		0		select,insert,update,references	<empty Comment>
dl	char(128)	utf8mb3_bin	NO		<empty Default>		select,insert,update,references	<empty Comment>
type	enum('function','aggregate')	utf8mb3_general_ci	NO		NULL		select,insert,update,references	<empty Comment>
func	0	PRIMARY	1	name	A	0	NULL	NULL		BTREE			YES	NULL
name	1	<empty COLUMN_DEFAULT>	NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	PRI		select,insert,update,references	<empty COLUMN_COMMENT>	<empty GENERATION_EXPRESSION>
ret	2	0	NO	tinyint	NULL	NULL	3	0	NULL	NULL	NULL	tinyint			select,insert,update,references	<empty COLUMN_COMMENT>	<empty GENERATION_EXPRESSION>
dl	3	<empty COLUMN_DEFAULT>	NO	char	128	384	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(128)			select,insert,update,references	<empty COLUMN_COMMENT>	<empty GENERATION_EXPRESSION>
type	4	NULL	NO	enum	9	27	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	enum('function','aggregate')			select,insert,update,references	<empty COLUMN_COMMENT>	<empty GENERATION_EXPRESSION>
PRIMARY	PRIMARY KEY	YES
PRIMARY	name	1	NULL	NULL	NULL	NULL
PRIMARY	func	NULL	NULL
PRIMARY	1	name	A	0	NULL	NULL		BTREE			YES	NULL
func	BASE TABLE	InnoDB	10	Dynamic	0	0	16384	0	0	4194304	NULL	1	1	1	utf8mb3_bin	1	row_format=DYNAMIC stats_persistent=0	User defined functions
func	InnoDB	10	Dynamic	0	0	16384	0	0	4194304	NULL	2026-05-27 21:13:23	NULL	NULL	utf8mb3_bin	NULL	row_format=DYNAMIC stats_persistent=0	User defined functions
-1
```

`SELECT * FROM mysql.func ORDER BY name` returns an empty result set with the
four `mysql.func` columns.

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_func_table_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_func_table|builtin_schema_table_directory|mysql_system_show_columns|mysql_system_show_index|information_schema_mysql_system_constraints|information_schema_mysql_system_statistics|mysql_component_table|mysql_system_stats_table_status)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_func_table_expectations.sh
git diff --check
cmake --workflow --preset check
```
