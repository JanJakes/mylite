# Baseline mysql.tables_priv Table

This slice adds metadata-compatible, read-only placeholder support for
`mysql.tables_priv`, the MySQL grant table that stores table-level privilege
assignments.

MyLite does not implement persisted table-level grants in this slice. The
table is exposed as an empty read-only compatibility placeholder so
applications can inspect the table shape, while grant storage, grant reload,
and privilege enforcement remain outside this slice.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, grant tables:
  <https://dev.mysql.com/doc/refman/8.4/en/grant-tables.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_tables_priv_table_expectations.sh`.

The MySQL manual identifies `mysql.tables_priv` as the grant table for
table-level privileges. Runtime checks against the target MySQL 8.4.9
container provide the exact column, primary-key, secondary-index, constraint,
and table-status metadata used by this slice.

## Supported Behavior

MyLite supports `mysql.tables_priv` as a read-only empty system-table
placeholder:

```sql
SELECT COUNT(*) FROM mysql.tables_priv;

USE mysql;
SELECT COUNT(*) FROM tables_priv;
```

Both forms return `0` in MyLite. Non-aggregate reads return MySQL-shaped column
labels and no rows.

The following metadata surfaces expose MySQL 8.4.9-shaped metadata:

- `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`
- `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`
- `INFORMATION_SCHEMA.COLUMNS`
- `INFORMATION_SCHEMA.STATISTICS`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`
- `INFORMATION_SCHEMA.TABLES`
- `SHOW TABLE STATUS`

## Metadata Shape

`mysql.tables_priv` has 8 columns in the target runtime. The first four columns
identify the grant scope, `Grantor` records the account that issued the grant,
`Timestamp` is maintained by MySQL, and the two privilege columns are `SET`
columns.

| Column | Type | Null | Key | Default | Collation |
| --- | --- | --- | --- | --- | --- |
| `Host` | `char(255)` | `NO` | `PRI` | `` | `ascii_general_ci` |
| `Db` | `char(64)` | `NO` | `PRI` | `` | `utf8mb3_bin` |
| `User` | `char(32)` | `NO` | `PRI` | `` | `utf8mb3_bin` |
| `Table_name` | `char(64)` | `NO` | `PRI` | `` | `utf8mb3_bin` |
| `Grantor` | `varchar(288)` | `NO` | `MUL` | `` | `utf8mb3_bin` |
| `Timestamp` | `timestamp` | `NO` | `` | `CURRENT_TIMESTAMP` | `NULL` |
| `Table_priv` | `set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter','Create View','Show view','Trigger')` | `NO` | `` | `` | `utf8mb3_general_ci` |
| `Column_priv` | `set('Select','Insert','Update','References')` | `NO` | `` | `` | `utf8mb3_general_ci` |

`Timestamp` has `EXTRA = DEFAULT_GENERATED on update CURRENT_TIMESTAMP`.

The target runtime contains 2 rows. MyLite intentionally returns an empty
placeholder because table grant descriptors, `GRANT`, `REVOKE`, grant table
reload, and privilege enforcement are outside this slice.

Observed stable table-status fields:

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
| `INDEX_LENGTH` / `Index_length` | `16384` |
| `DATA_FREE` / `Data_free` | `4194304` |
| `TABLE_COLLATION` / `Collation` | `utf8mb3_bin` |
| `CREATE_OPTIONS` / `Create_options` | `row_format=DYNAMIC stats_persistent=0` |
| `TABLE_COMMENT` / `Comment` | `Table privileges` |

The `PRIMARY` index has four key parts:

| Sequence | Column | Cardinality |
| ---: | --- | ---: |
| 1 | `Host` | 1 |
| 2 | `User` | 2 |
| 3 | `Db` | 2 |
| 4 | `Table_name` | 2 |

The nonunique secondary `Grantor` index has one key part on `Grantor` with
cardinality `2`. MySQL also exposes that secondary index through
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`, but not through
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` or `KEY_COLUMN_USAGE`.

`CREATE_TIME` is a non-`NULL` datetime string. `AUTO_INCREMENT`,
`UPDATE_TIME`, `CHECK_TIME`, and `CHECKSUM` are SQL `NULL`.

## Syntax

No parser change is required. Existing MyLite grammar already admits the
targeted statement shapes:

```lemon
select_stmt ::= SELECT select_options select_list from_clause select_tail.
from_clause ::= FROM table_factor.
table_factor ::= qualified_name alias_opt index_hint_list_opt.
cmd ::= SHOW show_columns_kind FROM qualified_name show_columns_tail.
cmd ::= DESCRIBE qualified_name.
cmd ::= SHOW show_index_kind FROM qualified_name show_index_tail.
cmd ::= SHOW TABLE STATUS show_table_status_tail.
cmd ::= USE identifier.
```

Grant and revoke statements remain out of scope.

## Diagnostics And Limits

- Writes remain rejected by the existing built-in schema write protection.
- Direct reads operate on an empty placeholder and do not expose MyLite's
  synthetic privileges as persisted table-level grant rows.
- No table grant catalog, column grant catalog, role catalog, grant reload,
  privilege filtering, privilege enforcement, account-management DDL,
  `SHOW GRANTS` expansion, or `FLUSH PRIVILEGES` behavior is added.
- No physical `mysql.tables_priv` table, SQLite virtual table, or SQLite fork
  patch is introduced.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: add a static mysql system-table definition and empty-row
  read support for `mysql.tables_priv`.
- Metadata: expose MySQL-shaped column, primary-key, secondary-index,
  constraint, statistics, and table-status metadata.
- Catalog/storage/SQLite: unchanged.

## Test Plan

- Add a MySQL expectation script that verifies target row count, column
  metadata, primary-key and secondary-index metadata, constraints, and
  table-status metadata.
- Add focused C runtime coverage for empty direct reads, selected-schema reads,
  columns, indexes, constraint metadata, table metadata, and table status.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_tables_priv_table_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_tables_priv_table_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_tables_priv_table_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_tables_priv_table$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
