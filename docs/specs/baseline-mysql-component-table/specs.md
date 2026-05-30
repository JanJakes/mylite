# Baseline mysql.component Table

This slice makes `mysql.component` a limited read-only synthetic system table.
MyLite already exposes the table name in the built-in `mysql` schema directory
and uses it in the persistent-statistics baselines. This feature adds direct
empty reads plus MySQL-shaped metadata for the table's columns, primary key,
table status, and related information-schema surfaces.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
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
  `packages/libmylite/tests/mysql_baseline_mysql_component_table_expectations.sh`.

The MySQL manual identifies `mysql.component` as the registry for installed
server components and states that `mysql` system tables use InnoDB unless a
specific table category says otherwise. Runtime checks against MySQL 8.4.9 show
that a fresh target runtime has no installed component rows, while ordinary
column, primary-key, and table-status introspection is visible.

## Supported Behavior

The supported direct-read forms are:

```sql
SELECT component_id, component_group_id, component_urn FROM mysql.component;

USE mysql;
SELECT component_id, component_group_id, component_urn FROM component;
```

The result has MySQL-shaped column labels and zero rows in MyLite's baseline
runtime. Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and
`LIMIT` behavior are inherited from the existing MySQL-system-table query
engine.

The supported metadata surfaces are:

```sql
SHOW COLUMNS FROM mysql.component;
SHOW FULL COLUMNS FROM mysql.component;
SHOW INDEX FROM mysql.component;

SELECT ... FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';

SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';

SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'component';

SHOW TABLE STATUS FROM mysql LIKE 'component';
```

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
`component` as a `BASE TABLE`; this slice does not change the built-in table
directory membership.

## Column Metadata

`mysql.component` has three columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `component_id` | `int unsigned` | `NO` | `PRI` | `NULL` | `auto_increment` | `NULL` |
| `component_group_id` | `int unsigned` | `NO` | `` | `NULL` | `` | `NULL` |
| `component_urn` | `text` | `NO` | `` | `NULL` | `` | `utf8mb3_general_ci` |

`INFORMATION_SCHEMA.COLUMNS` uses these MySQL 8.4.9 values:

- `ORDINAL_POSITION` values are `1`, `2`, and `3`;
- integer columns have `DATA_TYPE = 'int'`, `COLUMN_TYPE = 'int unsigned'`,
  `NUMERIC_PRECISION = 10`, and `NUMERIC_SCALE = 0`;
- `component_urn` has `DATA_TYPE = 'text'`, `COLUMN_TYPE = 'text'`,
  `CHARACTER_MAXIMUM_LENGTH = 65535`, `CHARACTER_OCTET_LENGTH = 65535`,
  `CHARACTER_SET_NAME = 'utf8mb3'`, and
  `COLLATION_NAME = 'utf8mb3_general_ci'`;
- `PRIVILEGES` is `select,insert,update,references`;
- `COLUMN_COMMENT` and `GENERATION_EXPRESSION` are empty strings.

## Key And Constraint Metadata

The only key is `PRIMARY(component_id)`.

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` expose one row:

- `NON_UNIQUE = 0`;
- `INDEX_NAME` / `Key_name` is `PRIMARY`;
- `SEQ_IN_INDEX = 1`;
- `COLUMN_NAME = 'component_id'`;
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
| `AUTO_INCREMENT` / `Auto_increment` | `1` |
| `TABLE_COLLATION` / `Collation` | `utf8mb3_general_ci` |
| `CREATE_OPTIONS` / `Create_options` | `row_format=DYNAMIC` |
| `TABLE_COMMENT` / `Comment` | `Components` |

`CREATE_TIME` is a non-`NULL` datetime string. `UPDATE_TIME`, `CHECK_TIME`, and
`CHECKSUM` are SQL `NULL`. MyLite renders `CREATE_TIME` from the current
statement timestamp for the synthetic row, matching the non-`NULL` shape
without introducing durable server-startup state.

## Diagnostics And Limits

- The baseline table is empty. MyLite does not implement `INSTALL COMPONENT`,
  `UNINSTALL COMPONENT`, component loading, persisted component rows, component
  services, or automatic startup loading.
- Writes to `mysql.component` remain blocked by the built-in schema write guard
  before catalog mutation.
- Other object-information system tables are governed by their own specs; this
  slice does not add plugin metadata.
- `SHOW CREATE TABLE mysql.component` remains out of scope for this slice.
- Privilege filtering is not implemented.
- No physical `mysql.component` table, SQLite virtual table, or SQLite fork
  patch is introduced.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `SELECT`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA`, and `SHOW TABLE STATUS` forms are reused.
- Analyzer/runtime: resolves `mysql.component` through the existing
  MySQL-system-table definition path and synthesizes zero direct rows plus
  metadata rows.
- Catalog metadata: unchanged. The table definition is static MyLite-owned
  system metadata and is not stored in user catalogs.
- Storage/VFS/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT COUNT(*) FROM mysql.component;
SHOW COLUMNS FROM mysql.component;
SHOW FULL COLUMNS FROM mysql.component;
SHOW INDEX FROM mysql.component;
SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
       COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
       GENERATION_EXPRESSION
  FROM information_schema.columns
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='component'
 ORDER BY ORDINAL_POSITION;
SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='component';
SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
       POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
       REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
  FROM information_schema.key_column_usage
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='component';
SELECT CONSTRAINT_NAME, TABLE_NAME, ENGINE_ATTRIBUTE,
       SECONDARY_ENGINE_ATTRIBUTE
  FROM information_schema.table_constraints_extensions
 WHERE CONSTRAINT_SCHEMA='mysql' AND TABLE_NAME='component';
SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, SUB_PART,
       PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE,
       EXPRESSION
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='component';
SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
       AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
       AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
       CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
       TABLE_COMMENT
  FROM information_schema.tables
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='component';
SHOW TABLE STATUS FROM mysql LIKE 'component';
SELECT ROW_COUNT();
```

Observed output:

```text
8.4.9
0
component_id	int unsigned	NO	PRI	NULL	auto_increment
component_group_id	int unsigned	NO		NULL	<empty Extra>
component_urn	text	NO		NULL	<empty Extra>
component_id	int unsigned	NULL	NO	PRI	NULL	auto_increment	select,insert,update,references	<empty Comment>
component_group_id	int unsigned	NULL	NO		NULL		select,insert,update,references	<empty Comment>
component_urn	text	utf8mb3_general_ci	NO		NULL		select,insert,update,references	<empty Comment>
component	0	PRIMARY	1	component_id	A	0	NULL	NULL		BTREE			YES	NULL
component_id	1	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	PRI	auto_increment	select,insert,update,references	<empty COLUMN_COMMENT>	<empty GENERATION_EXPRESSION>
component_group_id	2	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned			select,insert,update,references	<empty COLUMN_COMMENT>	<empty GENERATION_EXPRESSION>
component_urn	3	NULL	NO	text	65535	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	text			select,insert,update,references	<empty COLUMN_COMMENT>	<empty GENERATION_EXPRESSION>
PRIMARY	PRIMARY KEY	YES
PRIMARY	component_id	1	NULL	NULL	NULL	NULL
PRIMARY	component	NULL	NULL
PRIMARY	1	component_id	A	0	NULL	NULL		BTREE			YES	NULL
component	BASE TABLE	InnoDB	10	Dynamic	0	0	16384	0	0	4194304	1	1	1	1	utf8mb3_general_ci	1	row_format=DYNAMIC	Components
component	InnoDB	10	Dynamic	0	0	16384	0	0	4194304	1	2026-05-27 21:13:23	NULL	NULL	utf8mb3_general_ci	NULL	row_format=DYNAMIC	Components
-1
```

`SELECT * FROM mysql.component ORDER BY component_id` returns an empty result
set with the three component columns.

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_component_table_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_component_table|builtin_schema_table_directory|mysql_system_show_columns|mysql_system_show_index|information_schema_mysql_system_constraints|mysql_system_stats_table_status)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_component_table_expectations.sh
git diff --check
cmake --workflow --preset check
```
