# Baseline mysql.global_grants Table

This slice adds metadata-compatible, read-only placeholder support for
`mysql.global_grants`, the MySQL grant table that stores dynamic global
privilege assignments.

MyLite does not implement dynamic privilege storage in this slice. The table is
exposed as an empty read-only compatibility placeholder so applications can
inspect the table shape, while dynamic privilege assignment, grant reload, and
privilege enforcement remain outside this slice.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, grant tables:
  <https://dev.mysql.com/doc/refman/8.4/en/grant-tables.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_global_grants_table_expectations.sh`.

The MySQL manual identifies `mysql.global_grants` as the grant table for
dynamic global privileges. Runtime checks against the target MySQL 8.4.9
container provide the exact column, primary-key, constraint, and table-status
metadata used by this slice.

## Supported Behavior

MyLite supports `mysql.global_grants` as a read-only empty system-table
placeholder:

```sql
SELECT COUNT(*) FROM mysql.global_grants;

USE mysql;
SELECT COUNT(*) FROM global_grants;
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

`mysql.global_grants` has four columns in the target runtime:

| Column | Type | Null | Key | Default | Collation |
| --- | --- | --- | --- | --- | --- |
| `USER` | `char(32)` | `NO` | `PRI` | `` | `utf8mb3_bin` |
| `HOST` | `char(255)` | `NO` | `PRI` | `` | `ascii_general_ci` |
| `PRIV` | `char(32)` | `NO` | `PRI` | `` | `utf8mb3_general_ci` |
| `WITH_GRANT_OPTION` | `enum('N','Y')` | `NO` | `` | `N` | `utf8mb3_general_ci` |

The target runtime contains 96 rows. MyLite intentionally returns an empty
placeholder because persisted accounts, dynamic privilege assignments,
`GRANT`, `REVOKE`, roles, grant table reload, and privilege enforcement are
outside this slice.

Observed table-status fields:

| Field | Value |
| --- | --- |
| `TABLE_TYPE` / row type | `BASE TABLE` |
| `ENGINE` | `InnoDB` |
| `VERSION` | `10` |
| `ROW_FORMAT` | `Dynamic` |
| `TABLE_ROWS` / `Rows` | `103` |
| `AVG_ROW_LENGTH` / `Avg_row_length` | `795` |
| `DATA_LENGTH` / `Data_length` | `81920` |
| `MAX_DATA_LENGTH` / `Max_data_length` | `0` |
| `INDEX_LENGTH` / `Index_length` | `0` |
| `DATA_FREE` / `Data_free` | `4194304` |
| `TABLE_COLLATION` / `Collation` | `utf8mb3_bin` |
| `CREATE_OPTIONS` / `Create_options` | `row_format=DYNAMIC stats_persistent=0` |
| `TABLE_COMMENT` / `Comment` | `Extended global grants` |

The row-estimate, average-length, storage-length, free-space, and index
cardinality values are live InnoDB statistics. They can change on reused MySQL
comparison runtimes, so the MySQL expectation artifact checks their type/shape
but not fixed numeric values. MyLite's runtime tests verify the fixed empty
placeholder values exposed by MyLite.

The `PRIMARY` index has three key parts:

| Sequence | Column | Cardinality |
| ---: | --- | ---: |
| 1 | `USER` | 1 |
| 2 | `HOST` | 1 |
| 3 | `PRIV` | 103 |

`CREATE_TIME` is a non-`NULL` datetime string. `AUTO_INCREMENT`, `UPDATE_TIME`,
`CHECK_TIME`, and `CHECKSUM` are SQL `NULL`.

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
  synthetic runtime privileges as persisted dynamic privilege rows.
- No dynamic privilege catalog, role catalog, grant reload, privilege
  filtering, privilege enforcement, account-management DDL, `SHOW GRANTS`
  expansion, or `FLUSH PRIVILEGES` behavior is added.
- No physical `mysql.global_grants` table, SQLite virtual table, or SQLite fork
  patch is introduced.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: add a static mysql system-table definition and empty-row
  read support for `mysql.global_grants`.
- Metadata: expose MySQL-shaped column, primary-key, constraint, statistics,
  and table-status metadata.
- Catalog/storage/SQLite: unchanged.

## Test Plan

- Add a MySQL expectation script that verifies target row count, column
  metadata, primary-key metadata, constraints, and table-status metadata.
- Add focused C runtime coverage for empty direct reads, selected-schema reads,
  columns, indexes, constraint metadata, table metadata, and table status.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_global_grants_table_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_global_grants_table_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_global_grants_table_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_global_grants_table$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
