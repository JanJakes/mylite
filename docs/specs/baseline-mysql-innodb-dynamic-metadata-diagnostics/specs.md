# Baseline mysql.innodb_dynamic_metadata diagnostics

This slice adds MySQL-shaped diagnostics for the hidden
`mysql.innodb_dynamic_metadata` system table.

The target MySQL 8.4.9 runtime documents this table as an InnoDB-owned system
table for fast-changing table metadata. Runtime probes show that it is not a
publicly queryable `mysql` system table: direct reads and metadata statements
are rejected, and directory metadata omits the table.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, data dictionary schema:
  <https://dev.mysql.com/doc/refman/8.4/en/data-dictionary-schema.html>
- MySQL 8.4 Reference Manual, data dictionary usage differences:
  <https://dev.mysql.com/doc/refman/8.4/en/data-dictionary-usage-differences.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_data_dictionary_table_diagnostics_expectations.sh`.

The manual lists `innodb_dynamic_metadata` among miscellaneous `mysql` system
tables and describes it as InnoDB-owned metadata storage. The observed target
runtime protects it with the same `3554 / HY000` error number used for hidden
dictionary tables, but with `system table` wording.

## Supported Behavior

MyLite rejects direct reads of `mysql.innodb_dynamic_metadata`:

```sql
SELECT COUNT(*) FROM mysql.innodb_dynamic_metadata;
USE mysql;
SELECT COUNT(*) FROM innodb_dynamic_metadata;
```

The error is `3554 / HY000` with this message text:

```text
Access to system table 'mysql.innodb_dynamic_metadata' is rejected.
```

MyLite also rejects metadata statements that target the hidden table:

```sql
SHOW COLUMNS FROM mysql.innodb_dynamic_metadata;
DESCRIBE mysql.innodb_dynamic_metadata;
SHOW INDEX FROM mysql.innodb_dynamic_metadata;
```

The table remains absent from directory metadata:

```sql
SHOW FULL TABLES FROM mysql LIKE 'innodb_dynamic_metadata';
SHOW TABLE STATUS FROM mysql LIKE 'innodb_dynamic_metadata';
SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME = 'innodb_dynamic_metadata';
```

The `SHOW` statements return no rows and the `INFORMATION_SCHEMA.TABLES` count
is `0`.

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
```

This slice only changes post-parse name classification and diagnostics.

## Semantics

`innodb_dynamic_metadata` is not added to MyLite's queryable `mysql`
system-table registry. It does not receive synthetic columns, index metadata,
table status rows, direct placeholder rows, or storage.

MyLite classifies the name as a hidden MySQL system table when a direct read or
metadata statement resolves to `mysql.innodb_dynamic_metadata`, including an
unqualified reference while the selected schema is `mysql`. It then raises
`3554 / HY000` before MyLite catalog resolution.

The table remains excluded from:

- `SHOW TABLES FROM mysql`
- `SHOW FULL TABLES FROM mysql`
- `INFORMATION_SCHEMA.TABLES`
- `SHOW TABLE STATUS FROM mysql`

## Diagnostics And Limits

- `SELECT`, `SHOW COLUMNS`, `DESCRIBE`, and `SHOW INDEX` return
  `3554 / HY000`.
- Unknown non-hidden `mysql` names keep the existing unsupported or
  table-not-found diagnostics for their statement class.
- MyLite does not persist InnoDB dynamic metadata, auto-increment counter
  history, index tree corruption flags, or the old data dictionary buffer table
  state.
- No physical SQLite table, virtual table, or SQLite fork hook is added.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: extends hidden MySQL system-table name classification and
  diagnostics.
- Metadata: no new metadata rows are exposed.
- Catalog/storage/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe verifies:

```sql
SELECT COUNT(*) FROM mysql.innodb_dynamic_metadata;
SHOW COLUMNS FROM mysql.innodb_dynamic_metadata;
DESC mysql.innodb_dynamic_metadata;
SHOW INDEX FROM mysql.innodb_dynamic_metadata;
USE mysql;
SELECT COUNT(*) FROM innodb_dynamic_metadata;
SELECT COUNT(*)
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME = 'innodb_dynamic_metadata';
SHOW FULL TABLES FROM mysql LIKE 'innodb_dynamic_metadata';
SHOW TABLE STATUS FROM mysql LIKE 'innodb_dynamic_metadata';
```

The target runtime returned `3554 / HY000` with `system table` wording for
the direct and metadata access forms, and returned no directory rows.

## Test Plan

- Extend the MySQL expectation script for hidden mysql-system table diagnostics
  to verify direct reads, selected-schema reads, representative metadata
  statements, and directory absence for `innodb_dynamic_metadata`.
- Extend focused C runtime coverage for the same diagnostics through the
  existing mysql data dictionary diagnostics test target.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_data_dictionary_table_diagnostics_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_data_dictionary_table_diagnostics_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_data_dictionary_table_diagnostics_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_data_dictionary_table_diagnostics$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
