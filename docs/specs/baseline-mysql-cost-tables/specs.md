# Baseline mysql Cost Tables

This slice makes `mysql.server_cost` and `mysql.engine_cost` limited read-only
synthetic system tables. MyLite already lists both names in the built-in
`mysql` schema directory; this feature adds direct reads, MySQL-shaped column
metadata, primary-key metadata, table status, and related information-schema
rows for the default optimizer cost constants observed in MySQL 8.4.9.

## Compatibility Authority

- MySQL 8.4 Reference Manual, optimizer cost model:
  <https://dev.mysql.com/doc/refman/8.4/en/cost-model.html>
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
  `packages/libmylite/tests/mysql_baseline_mysql_cost_tables_expectations.sh`.

The MySQL manual describes the optimizer cost model as using default compiled
cost constants plus optional rows from `mysql.server_cost` and
`mysql.engine_cost`. A `NULL` `cost_value` means the compiled default is used.
The `default_value` column is a read-only generated column.

## Supported Behavior

The supported direct-read forms are:

```sql
SELECT cost_name, cost_value, default_value
  FROM mysql.server_cost
 ORDER BY cost_name;

SELECT engine_name, device_type, cost_name, cost_value, default_value
  FROM mysql.engine_cost
 ORDER BY engine_name, device_type, cost_name;

USE mysql;
SELECT cost_name FROM server_cost WHERE default_value IS NOT NULL ORDER BY cost_name;
```

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior are inherited from the existing MySQL-system-table query engine.

`mysql.server_cost` returns six static rows with `cost_value = NULL`,
`comment = NULL`, and generated defaults:

| cost_name | default_value |
| --- | ---: |
| `disk_temptable_create_cost` | `20` |
| `disk_temptable_row_cost` | `0.5` |
| `key_compare_cost` | `0.05` |
| `memory_temptable_create_cost` | `1` |
| `memory_temptable_row_cost` | `0.1` |
| `row_evaluate_cost` | `0.1` |

`mysql.engine_cost` returns two static rows with `engine_name = 'default'`,
`device_type = 0`, `cost_value = NULL`, `comment = NULL`, and generated
defaults:

| cost_name | default_value |
| --- | ---: |
| `io_block_read_cost` | `1` |
| `memory_block_read_cost` | `0.25` |

The `last_update` result value is a non-`NULL` MySQL datetime. MyLite renders
the current statement timestamp for synthetic rows, matching the visible shape
without durable server startup or cost-table update state.

The supported metadata surfaces are:

```sql
SHOW COLUMNS FROM mysql.server_cost;
SHOW FULL COLUMNS FROM mysql.engine_cost;
SHOW INDEX FROM mysql.server_cost;
SHOW INDEX FROM mysql.engine_cost;

SELECT ... FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('server_cost', 'engine_cost');

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('server_cost', 'engine_cost');

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('server_cost', 'engine_cost');

SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('server_cost', 'engine_cost');

SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('server_cost', 'engine_cost');

SHOW TABLE STATUS FROM mysql LIKE 'server_cost';
SHOW TABLE STATUS FROM mysql LIKE 'engine_cost';
```

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
both names as `BASE TABLE`; this slice does not change built-in table directory
membership.

## Column Metadata

`mysql.server_cost` has five columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `cost_name` | `varchar(64)` | `NO` | `PRI` | `NULL` | `` | `utf8mb3_general_ci` |
| `cost_value` | `float` | `YES` | `` | `NULL` | `` | `NULL` |
| `last_update` | `timestamp` | `NO` | `` | `CURRENT_TIMESTAMP` | `DEFAULT_GENERATED on update CURRENT_TIMESTAMP` | `NULL` |
| `comment` | `varchar(1024)` | `YES` | `` | `NULL` | `` | `utf8mb3_general_ci` |
| `default_value` | `float` | `YES` | `` | `NULL` | `VIRTUAL GENERATED` | `NULL` |

`mysql.engine_cost` has seven columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `engine_name` | `varchar(64)` | `NO` | `PRI` | `NULL` | `` | `utf8mb3_general_ci` |
| `device_type` | `int` | `NO` | `PRI` | `NULL` | `` | `NULL` |
| `cost_name` | `varchar(64)` | `NO` | `PRI` | `NULL` | `` | `utf8mb3_general_ci` |
| `cost_value` | `float` | `YES` | `` | `NULL` | `` | `NULL` |
| `last_update` | `timestamp` | `NO` | `` | `CURRENT_TIMESTAMP` | `DEFAULT_GENERATED on update CURRENT_TIMESTAMP` | `NULL` |
| `comment` | `varchar(1024)` | `YES` | `` | `NULL` | `` | `utf8mb3_general_ci` |
| `default_value` | `float` | `YES` | `` | `NULL` | `VIRTUAL GENERATED` | `NULL` |

`INFORMATION_SCHEMA.COLUMNS` uses MySQL 8.4.9 metadata for ordinal positions,
character lengths, numeric precision, datetime precision, charset/collation,
`COLUMN_TYPE`, `COLUMN_KEY`, `EXTRA`, privileges, comments, and the generated
column expressions. The generated expressions are stored as MySQL reports them:

- `server_cost.default_value`:
  `(case \`cost_name\` when _utf8mb4\\'disk_temptable_create_cost\\' then 20.0 when _utf8mb4\\'disk_temptable_row_cost\\' then 0.5 when _utf8mb4\\'key_compare_cost\\' then 0.05 when _utf8mb4\\'memory_temptable_create_cost\\' then 1.0 when _utf8mb4\\'memory_temptable_row_cost\\' then 0.1 when _utf8mb4\\'row_evaluate_cost\\' then 0.1 else NULL end)`;
- `engine_cost.default_value`:
  `(case \`cost_name\` when _utf8mb4\\'io_block_read_cost\\' then 1.0 when _utf8mb4\\'memory_block_read_cost\\' then 0.25 else NULL end)`.

All other columns have an empty `GENERATION_EXPRESSION`.

## Key And Constraint Metadata

`mysql.server_cost` has `PRIMARY(cost_name)`.

`mysql.engine_cost` has `PRIMARY(cost_name, engine_name, device_type)`. This
primary-key order differs from the physical column order and must be preserved
in `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`, and
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE`.

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` expose BTREE primary-key rows
with `NON_UNIQUE = 0`, `COLLATION = 'A'`, `SUB_PART`, `PACKED`, and
`EXPRESSION` as SQL `NULL`, empty storage-engine comments, empty index
comments, `IS_VISIBLE = 'YES'`, and observed cardinality placeholders:

- `server_cost.PRIMARY`: cardinality `6`;
- `engine_cost.PRIMARY`: cardinality `2` for each key part.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes one enforced primary-key row per
table. `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes ordered primary-key column
rows with no referenced table or column values. `TABLE_CONSTRAINTS_EXTENSIONS`
includes one `NULL`-attribute primary-key row per table.

## Table Status

`INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose:

| Table | Rows | Avg row length | Data length | Data free | Update time |
| --- | ---: | ---: | ---: | ---: | --- |
| `server_cost` | `6` | `2730` | `16384` | `4194304` | `NULL` |
| `engine_cost` | `2` | `8192` | `16384` | `4194304` | `NULL` |

Both tables report `ENGINE = 'InnoDB'`, `VERSION = 10`,
`ROW_FORMAT = 'Dynamic'`, `MAX_DATA_LENGTH = 0`, `INDEX_LENGTH = 0`,
`AUTO_INCREMENT = NULL`, `TABLE_COLLATION = 'utf8mb3_general_ci'`,
`CHECK_TIME = NULL`, `CHECKSUM = NULL`, empty table comments, and
`CREATE_OPTIONS = 'row_format=DYNAMIC stats_persistent=0'`. `CREATE_TIME` is
non-`NULL` and rendered from the current statement timestamp in MyLite.

## Diagnostics And Limits

- The rows are the MySQL 8.4.9 built-in default rows only. MyLite does not
  persist user-added or user-updated cost rows.
- `cost_value` remains `NULL`; MyLite does not provide mutable optimizer cost
  overrides.
- `FLUSH OPTIMIZER_COSTS` and optimizer behavior changes are out of scope.
- Writes to both tables remain blocked by the built-in schema write guard
  before catalog mutation.
- `SHOW CREATE TABLE mysql.server_cost` and
  `SHOW CREATE TABLE mysql.engine_cost` remain out of scope for this slice.
- Privilege filtering is not implemented.
- No physical `mysql` cost table, SQLite virtual table, optimizer cost reload,
  or SQLite fork patch is introduced.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `SELECT`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA`, and `SHOW TABLE STATUS` forms are reused.
- Analyzer/runtime: resolves both tables through the existing
  MySQL-system-table definition path and synthesizes static rows plus metadata
  rows.
- Catalog metadata: unchanged. The table definitions and rows are static
  MyLite-owned system metadata and are not stored in user catalogs.
- Storage/VFS/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT cost_name, COALESCE(CAST(cost_value AS CHAR), 'NULL'),
       last_update IS NOT NULL, comment,
       COALESCE(CAST(default_value AS CHAR), 'NULL')
  FROM mysql.server_cost
 ORDER BY cost_name;
SELECT engine_name, device_type, cost_name,
       COALESCE(CAST(cost_value AS CHAR), 'NULL'),
       last_update IS NOT NULL, comment,
       COALESCE(CAST(default_value AS CHAR), 'NULL')
  FROM mysql.engine_cost
 ORDER BY engine_name, device_type, cost_name;
SHOW FULL COLUMNS FROM mysql.server_cost;
SHOW FULL COLUMNS FROM mysql.engine_cost;
SHOW INDEX FROM mysql.server_cost;
SHOW INDEX FROM mysql.engine_cost;
SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
       COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
       GENERATION_EXPRESSION
  FROM information_schema.columns
 WHERE TABLE_SCHEMA='mysql'
   AND TABLE_NAME IN ('server_cost','engine_cost')
 ORDER BY TABLE_NAME, ORDINAL_POSITION;
```

Observed output summary:

```text
8.4.9
server_cost rows:
  disk_temptable_create_cost NULL non-null-time NULL 20
  disk_temptable_row_cost NULL non-null-time NULL 0.5
  key_compare_cost NULL non-null-time NULL 0.05
  memory_temptable_create_cost NULL non-null-time NULL 1
  memory_temptable_row_cost NULL non-null-time NULL 0.1
  row_evaluate_cost NULL non-null-time NULL 0.1
engine_cost rows:
  default 0 io_block_read_cost NULL non-null-time NULL 1
  default 0 memory_block_read_cost NULL non-null-time NULL 0.25
indexes:
  server_cost PRIMARY(cost_name), cardinality 6
  engine_cost PRIMARY(cost_name, engine_name, device_type), cardinality 2
status:
  server_cost Rows 6, Avg_row_length 2730, Data_free 4194304,
  Create_time non-NULL, Update_time NULL
  engine_cost Rows 2, Avg_row_length 8192, Data_free 4194304,
  Create_time non-NULL, Update_time NULL
```

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_cost_tables_test
ctest --preset dev -R '^libmylite\.runtime\.mysql_cost_tables$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_cost_tables_expectations.sh
git diff --check
cmake --workflow --preset check
```
