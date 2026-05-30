# Baseline mysql.plugin Table

This slice makes `mysql.plugin` a limited read-only synthetic system table.
MyLite already lists the table in the built-in `mysql` schema directory. This
feature adds direct reads for the target runtime's connection-control plugin
registry rows plus MySQL-shaped column metadata, primary-key metadata, table
status, and related information-schema rows.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, installing and uninstalling plugins:
  <https://dev.mysql.com/doc/refman/8.4/en/plugin-loading.html>
- MySQL 8.4 Reference Manual, `INSTALL PLUGIN`:
  <https://dev.mysql.com/doc/refman/8.4/en/install-plugin.html>
- MySQL 8.4 Reference Manual, `UNINSTALL PLUGIN`:
  <https://dev.mysql.com/doc/refman/8.4/en/uninstall-plugin.html>
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
  `packages/libmylite/tests/mysql_baseline_mysql_plugin_table_expectations.sh`.

The MySQL manual describes `mysql.plugin` as the registry for non-built-in
server plugins that should be loaded during normal server startup. `INSTALL
PLUGIN` registers plugin name and library rows in this table, and `UNINSTALL
PLUGIN` removes rows from it. Runtime checks against the target MySQL 8.4.9
container ensure the connection-control plugins are installed before recording
expectations; the observed table has the two `connection_control.so` registry
rows used by existing connection-control metadata expectations.

## Supported Behavior

The supported direct-read forms are:

```sql
SELECT name, dl FROM mysql.plugin ORDER BY name;

USE mysql;
SELECT name, dl FROM plugin WHERE name LIKE 'CONNECTION%' ORDER BY name;
```

MyLite returns two static rows:

| name | dl |
| --- | --- |
| `CONNECTION_CONTROL` | `connection_control.so` |
| `CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` | `connection_control.so` |

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior are inherited from the existing MySQL-system-table query engine.

The supported metadata surfaces are:

```sql
SHOW COLUMNS FROM mysql.plugin;
SHOW FULL COLUMNS FROM mysql.plugin;
SHOW INDEX FROM mysql.plugin;

SELECT ... FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';

SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';

SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'plugin';

SHOW TABLE STATUS FROM mysql LIKE 'plugin';
```

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
`plugin` as a `BASE TABLE`; this slice does not change the built-in table
directory membership.

## Column Metadata

`mysql.plugin` has two columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `name` | `varchar(64)` | `NO` | `PRI` | `` | `` | `utf8mb3_general_ci` |
| `dl` | `varchar(128)` | `NO` | `` | `` | `` | `utf8mb3_general_ci` |

`INFORMATION_SCHEMA.COLUMNS` uses these MySQL 8.4.9 values:

- `ORDINAL_POSITION` values are `1` and `2`;
- `name` has `DATA_TYPE = 'varchar'`, `COLUMN_TYPE = 'varchar(64)'`,
  `CHARACTER_MAXIMUM_LENGTH = 64`, `CHARACTER_OCTET_LENGTH = 192`,
  `CHARACTER_SET_NAME = 'utf8mb3'`, and
  `COLLATION_NAME = 'utf8mb3_general_ci'`;
- `dl` has `DATA_TYPE = 'varchar'`, `COLUMN_TYPE = 'varchar(128)'`,
  `CHARACTER_MAXIMUM_LENGTH = 128`, `CHARACTER_OCTET_LENGTH = 384`,
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
- `CARDINALITY = 1`;
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
| `TABLE_ROWS` / `Rows` | `2` |
| `AVG_ROW_LENGTH` / `Avg_row_length` | `8192` |
| `DATA_LENGTH` / `Data_length` | `16384` |
| `MAX_DATA_LENGTH` / `Max_data_length` | `0` |
| `INDEX_LENGTH` / `Index_length` | `0` |
| `DATA_FREE` / `Data_free` | `4194304` |
| `AUTO_INCREMENT` / `Auto_increment` | `NULL` |
| `TABLE_COLLATION` / `Collation` | `utf8mb3_general_ci` |
| `CREATE_OPTIONS` / `Create_options` | `row_format=DYNAMIC stats_persistent=0` |
| `TABLE_COMMENT` / `Comment` | `MySQL plugins` |

`CREATE_TIME` and `UPDATE_TIME` are non-`NULL` datetime strings. `CHECK_TIME`
and `CHECKSUM` are SQL `NULL`. MyLite renders both visible timestamps from the
current statement timestamp for the synthetic row, matching the non-`NULL`
shape without introducing durable server-startup or plugin-install state.

## Diagnostics And Limits

- The baseline table contains the two connection-control plugin registry rows
  used by the target MySQL 8.4.9 comparison runtime. MyLite does not load or
  execute either plugin.
- `SHOW PLUGINS` and `INFORMATION_SCHEMA.PLUGINS` remain the existing limited
  one-row InnoDB plugin surface. This slice does not expand the plugin
  inventory rows there.
- `INSTALL PLUGIN`, `UNINSTALL PLUGIN`, startup plugin loading, plugin
  activation state, plugin options, plugin-library path probing, and persisted
  plugin changes are out of scope.
- Writes to `mysql.plugin` remain blocked by the built-in schema write guard
  before catalog mutation.
- `SHOW CREATE TABLE mysql.plugin` remains out of scope for this slice.
- Privilege filtering is not implemented.
- No physical `mysql.plugin` table, SQLite virtual table, plugin loader, or
  SQLite fork patch is introduced.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `SELECT`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA`, and `SHOW TABLE STATUS` forms are reused.
- Analyzer/runtime: resolves `mysql.plugin` through the existing
  MySQL-system-table definition path and synthesizes static rows plus metadata
  rows.
- Catalog metadata: unchanged. The table definition and rows are static
  MyLite-owned system metadata and are not stored in user catalogs.
- Storage/VFS/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
INSTALL PLUGIN CONNECTION_CONTROL SONAME 'connection_control.so';
INSTALL PLUGIN CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS
  SONAME 'connection_control.so';
SELECT name, dl FROM mysql.plugin ORDER BY name;
SHOW FULL COLUMNS FROM mysql.plugin;
SHOW INDEX FROM mysql.plugin;
SELECT COUNT(*) FROM mysql.plugin;
SHOW TABLE STATUS FROM mysql LIKE 'plugin';
SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
       COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
       GENERATION_EXPRESSION
  FROM information_schema.columns
 WHERE TABLE_SCHEMA='mysql' AND TABLE_NAME='plugin'
 ORDER BY ORDINAL_POSITION;
```

The expectation script installs the connection-control plugins only when the
target runtime does not already report them as active. Observed output,
summarized with empty trailing strings rendered as `<empty>`:

```text
8.4.9
plugin rows:
  CONNECTION_CONTROL connection_control.so
  CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS connection_control.so
columns: name varchar(64) primary key, dl varchar(128)
SHOW INDEX: PRIMARY(name), cardinality 1
COUNT(*): 2
status: InnoDB, version 10, Dynamic, Rows 2, Avg_row_length 8192,
  Data_length 16384, Data_free 4194304, create/update time non-NULL,
  collation utf8mb3_general_ci, create options row_format=DYNAMIC
  stats_persistent=0, comment MySQL plugins
```

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_plugin_table_test
ctest --preset dev -R '^libmylite\.runtime\.mysql_plugin_table$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_plugin_table_expectations.sh
git diff --check
cmake --workflow --preset check
```
