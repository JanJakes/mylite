# Baseline mysql.user Table

This slice adds metadata-compatible, read-only placeholder support for
`mysql.user`, the MySQL grant table that stores account scope columns, static
global privileges, and account attributes.

MyLite does not implement a physical account store in this slice. The table is
exposed as an empty read-only compatibility placeholder so applications that
inspect the table shape can proceed, while authentication, account management,
and privilege enforcement remain governed by existing MyLite session behavior.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, grant tables:
  <https://dev.mysql.com/doc/refman/8.4/en/grant-tables.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_user_table_expectations.sh`.

The MySQL manual identifies `mysql.user` as the grant table for user accounts,
static global privileges, and other nonprivilege account columns. Runtime checks
against the target MySQL 8.4.9 container provide the exact column, primary-key,
constraint, and table-status metadata used by this slice.

## Supported Behavior

MyLite supports `mysql.user` as a read-only empty system-table placeholder:

```sql
SELECT COUNT(*) FROM mysql.user;

USE mysql;
SELECT COUNT(*) FROM user;
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

`mysql.user` has 51 columns in the target runtime. The first two columns,
`Host` and `User`, form the composite primary key in that order. The remaining
columns describe static global privileges, SSL requirements, resource limits,
authentication data, password metadata, role-management privileges, password
reuse policy, current-password requirements, and JSON account attributes.

The target runtime contains five rows. MyLite intentionally returns an empty
placeholder because persisted MySQL accounts, authentication plugins, password
hashes, grant reload behavior, `CREATE USER`, `ALTER USER`, `DROP USER`,
`GRANT`, `REVOKE`, roles, and privilege enforcement are outside this slice.

Observed stable table-status fields:

| Field | Value |
| --- | --- |
| `TABLE_TYPE` / row type | `BASE TABLE` |
| `ENGINE` | `InnoDB` |
| `VERSION` | `10` |
| `ROW_FORMAT` | `Dynamic` |
| `TABLE_ROWS` / `Rows` | `5` |
| `AVG_ROW_LENGTH` / `Avg_row_length` | `3276` |
| `DATA_LENGTH` / `Data_length` | `16384` |
| `MAX_DATA_LENGTH` / `Max_data_length` | `0` |
| `INDEX_LENGTH` / `Index_length` | `0` |
| `DATA_FREE` / `Data_free` | `4194304` |
| `TABLE_COLLATION` / `Collation` | `utf8mb3_bin` |
| `CREATE_OPTIONS` / `Create_options` | `row_format=DYNAMIC stats_persistent=0` |
| `TABLE_COMMENT` / `Comment` | `Users and global privileges` |

The `PRIMARY` index has two key parts:

| Sequence | Column | Cardinality |
| ---: | --- | ---: |
| 1 | `Host` | 2 |
| 2 | `User` | 5 |

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

Account-management and privilege statements remain out of scope.

## Diagnostics And Limits

- Writes remain rejected by the existing built-in schema write protection.
- Direct reads operate on an empty placeholder and do not expose MyLite's
  synthetic runtime identity as a persisted account row.
- No authentication plugin lifecycle, password hashing, role catalog, grant
  table reload, privilege filtering, account-management DDL, `SHOW CREATE
  USER`, or `FLUSH PRIVILEGES` behavior is added.
- No physical `mysql.user` table, SQLite virtual table, or SQLite fork patch is
  introduced.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: add a static mysql system-table definition and empty-row
  read support for `mysql.user`.
- Metadata: expose MySQL-shaped column, primary-key, constraint, statistics,
  and table-status metadata.
- Catalog/storage/SQLite: unchanged.

## Test Plan

- Add a MySQL expectation script that verifies target row count, column
  metadata, primary-key metadata, constraints, and table-status metadata.
- Add focused C runtime coverage for empty direct reads, selected-schema reads,
  columns, indexes, constraint metadata, table metadata, and table status.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_user_table_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_user_table_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_user_table_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_user_table$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
