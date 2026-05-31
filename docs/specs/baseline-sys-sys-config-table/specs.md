# Baseline sys.sys_config Table

This slice adds a MySQL-shaped `sys.sys_config` baseline. MyLite exposes the
table as a small read-only synthetic table with the default MySQL 8.4.9 sys
configuration rows, column metadata, primary-key metadata, and table-status
metadata. It does not implement persistent sys-schema configuration storage,
sys functions, or sys triggers.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.sys_config`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-sys-config.html>
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
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-extensions-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_sys_config_table_expectations.sh`.

The MySQL manual describes `sys.sys_config` as the table that stores sys schema
configuration options, their values, the last modification time, and the
account that last changed each row. MySQL sys functions can prefer session
variables named with an `@sys.` prefix over table values. This slice exposes
the table shape and defaults only; it does not implement sys functions or
session-variable fallback behavior.

## Supported Behavior

Supported direct reads:

```sql
SELECT variable, value, set_time, set_by
  FROM sys.sys_config
 ORDER BY variable;

USE sys;
SELECT value FROM sys_config WHERE variable = 'statement_truncate_len';
SELECT COUNT(*) FROM sys_config;
```

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior are inherited from the existing MyLite synthetic system-table query
engine. Reads report zero affected rows and preserve MySQL's `ROW_COUNT() = -1`
post-select behavior.

The table returns these six default rows:

| variable | value | set_time | set_by |
| --- | --- | --- | --- |
| `diagnostics.allow_i_s_tables` | `OFF` | non-`NULL` timestamp | `NULL` |
| `diagnostics.include_raw` | `OFF` | non-`NULL` timestamp | `NULL` |
| `ps_thread_trx_info.max_length` | `65535` | non-`NULL` timestamp | `NULL` |
| `statement_performance_analyzer.limit` | `100` | non-`NULL` timestamp | `NULL` |
| `statement_performance_analyzer.view` | `NULL` | non-`NULL` timestamp | `NULL` |
| `statement_truncate_len` | `64` | non-`NULL` timestamp | `NULL` |

MySQL stores an installation-time `set_time` value. MyLite has no durable
server-installation sys table, so it renders the current statement timestamp
for these synthetic rows while preserving the non-`NULL` shape and `set_by =
NULL` default.

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
`sys.sys_config` as `BASE TABLE`.

## Column Metadata

`sys.sys_config` has four columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `variable` | `varchar(128)` | `NO` | `PRI` | `NULL` | `` | `utf8mb4_0900_ai_ci` |
| `value` | `varchar(128)` | `YES` | `` | `NULL` | `` | `utf8mb4_0900_ai_ci` |
| `set_time` | `timestamp` | `YES` | `` | `CURRENT_TIMESTAMP` | `DEFAULT_GENERATED on update CURRENT_TIMESTAMP` | `NULL` |
| `set_by` | `varchar(128)` | `YES` | `` | `NULL` | `` | `utf8mb4_0900_ai_ci` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, and `DESCRIBE` expose the same shape.
`SHOW FULL COLUMNS` reports fixed privileges
`select,insert,update,references` and empty comments. `INFORMATION_SCHEMA.COLUMNS`
reports MySQL 8.4.9 ordinal positions, defaults, nullability, character lengths,
data type, column type, key marker, `EXTRA`, privileges, empty comments, and
empty generation expressions.

## Key And Constraint Metadata

The only key is:

```sql
PRIMARY KEY (variable)
```

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` expose one visible BTREE
primary-key row:

- `NON_UNIQUE = 0`
- `INDEX_NAME = PRIMARY`
- `SEQ_IN_INDEX = 1`
- `COLUMN_NAME = variable`
- `COLLATION = A`
- `CARDINALITY = 6`
- `NULLABLE = ''`
- `SUB_PART`, `PACKED`, and `EXPRESSION` are SQL `NULL`
- `COMMENT` and `INDEX_COMMENT` are empty strings
- `IS_VISIBLE = YES`

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes one enforced primary-key row.
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes the ordered `variable` key-part
row with no referenced table or column values. `TABLE_CONSTRAINTS_EXTENSIONS`
includes one `NULL`-attribute primary-key row.

## Table Status

`INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose the MySQL-observed
target-runtime status shape for `sys.sys_config`:

| Field | Value |
| --- | --- |
| `TABLE_TYPE` / table status type | `BASE TABLE` |
| `ENGINE` | `InnoDB` |
| `VERSION` | `10` |
| `ROW_FORMAT` | `Dynamic` |
| `TABLE_ROWS` / `Rows` | `6` |
| `AVG_ROW_LENGTH` / `Avg_row_length` | `2730` |
| `DATA_LENGTH` / `Data_length` | `16384` |
| `MAX_DATA_LENGTH` / `Max_data_length` | `0` |
| `INDEX_LENGTH` / `Index_length` | `0` |
| `DATA_FREE` / `Data_free` | `0` |
| `AUTO_INCREMENT` / `Auto_increment` | `NULL` |
| `CREATE_TIME` / `Create_time` | non-`NULL` timestamp |
| `UPDATE_TIME` / `Update_time` | `NULL` |
| `CHECK_TIME` / `Check_time` | `NULL` |
| `TABLE_COLLATION` / `Collation` | `utf8mb4_0900_ai_ci` |
| `CHECKSUM` / `Checksum` | `NULL` |
| `CREATE_OPTIONS` / `Create_options` | empty string |
| `TABLE_COMMENT` / `Comment` | empty string |

`CREATE_TIME` follows the existing built-in status policy and uses the current
statement timestamp for the synthetic row.

## Unsupported Behavior

This slice intentionally does not implement:

- persistent or writable `sys.sys_config` storage;
- insert/update/delete semantics or trigger side effects;
- `sys.sys_config_insert_set_user` and `sys.sys_config_update_set_user`
  metadata in `INFORMATION_SCHEMA.TRIGGERS` or `SHOW TRIGGERS`;
- sys functions, procedures, views, or their `@sys.` session-variable fallback;
- Performance Schema-backed metrics or diagnostics;
- privilege filtering or account-specific sys table visibility.

Writes to `sys.sys_config` continue to use MyLite's built-in schema write guard.

## Parser And Grammar

No new SQL grammar is required. The feature uses existing qualified and
selected-schema table references, existing `SHOW COLUMNS`, `SHOW INDEX`, and
`SHOW TABLE STATUS` syntax, and existing `INFORMATION_SCHEMA` query support.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the existing synthetic system-table descriptor
  table with a `schema_name = 'sys'` entry.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and result builder for direct reads.
- SHOW metadata: reuses the existing MySQL-system-table column/index rendering
  path after resolving the explicit `sys.sys_config` descriptor.
- Information schema: existing synthetic `COLUMNS`, `STATISTICS`,
  `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `TABLE_CONSTRAINTS_EXTENSIONS` loops consume the descriptor.
- Storage/SQLite: unchanged. No physical table, trigger, view, index, or SQLite
  fork hook is required.

## Performance

The table has six static rows and four columns. Direct reads materialize a
small bounded row set before applying the existing metadata-query filter and
projection logic. The information-schema and SHOW additions add one descriptor
to existing bounded built-in metadata loops.

## Tests

MySQL 8.4.9 expectation coverage:

- direct default rows and `COUNT(*)`;
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, and `SHOW INDEX`;
- `INFORMATION_SCHEMA.COLUMNS`, `STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, `TABLE_CONSTRAINTS_EXTENSIONS`, and `TABLES`;
- `SHOW TABLE STATUS` with dynamic timestamp validation.

MyLite runtime coverage:

- direct qualified and selected-schema reads;
- row-count status after a synthetic select;
- default rows with non-`NULL` timestamp shape;
- SHOW and information-schema metadata;
- table-status rows and the unsupported `sys.version` column-catalog guard.
