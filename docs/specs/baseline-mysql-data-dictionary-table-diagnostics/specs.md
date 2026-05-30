# Baseline mysql Data Dictionary Table Diagnostics

This slice adds MySQL-shaped access diagnostics for hidden `mysql` data
dictionary tables. MySQL 8.4 exposes dictionary metadata through
`INFORMATION_SCHEMA` views and `SHOW` statements, but direct access to the
dictionary base tables is rejected.

The slice covers these hidden `mysql` names:

- `catalogs`
- `character_sets`
- `check_constraints`
- `collations`
- `column_statistics`
- `column_type_elements`
- `columns`
- `dd_properties`
- `events`
- `foreign_keys`
- `foreign_key_column_usage`
- `index_column_usage`
- `index_partitions`
- `index_stats`
- `indexes`
- `innodb_ddl_log`
- `parameter_type_elements`
- `parameters`
- `resource_groups`
- `routines`
- `schemata`
- `st_spatial_reference_systems`
- `table_partition_values`
- `table_partitions`
- `table_stats`
- `tables`
- `tablespace_files`
- `tablespaces`
- `triggers`
- `view_routine_usage`
- `view_table_usage`

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, data dictionary schema:
  <https://dev.mysql.com/doc/refman/8.4/en/data-dictionary-schema.html>
- MySQL 8.4 Reference Manual, data dictionary usage differences:
  <https://dev.mysql.com/doc/refman/8.4/en/data-dictionary-usage-differences.html>
- MySQL 8.4 Reference Manual, information schema and data dictionary
  integration:
  <https://dev.mysql.com/doc/refman/8.4/en/data-dictionary-information-schema.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_data_dictionary_table_diagnostics_expectations.sh`.

The MySQL manual describes data dictionary tables as protected and generally
invisible. They cannot be read directly, are not listed by `SHOW TABLES`, and
are not listed by `INFORMATION_SCHEMA.TABLES`. Applications should use the
corresponding `INFORMATION_SCHEMA` views and `SHOW` surfaces instead.

## Supported Behavior

MyLite rejects direct reads of the covered hidden names with MySQL's observed
diagnostic shape:

```sql
SELECT COUNT(*) FROM mysql.catalogs;
SELECT * FROM mysql.schemata;
USE mysql;
SELECT COUNT(*) FROM tables;
```

The error is `3554 / HY000`. The message text is:

- `Access to data dictionary table 'mysql.<name>' is rejected.` for all covered
  names except `mysql.innodb_ddl_log`;
- `Access to system table 'mysql.innodb_ddl_log' is rejected.` for
  `mysql.innodb_ddl_log`.

MyLite also rejects metadata statements that target those hidden names:

```sql
SHOW COLUMNS FROM mysql.catalogs;
DESCRIBE mysql.schemata;
SHOW INDEX FROM mysql.tables;
```

The diagnostic is the same `3554 / HY000` error and message used by direct
reads.

Hidden data dictionary tables remain absent from directory surfaces:

```sql
SHOW FULL TABLES FROM mysql LIKE 'schemata';

SELECT TABLE_NAME
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('catalogs', 'schemata', 'tables');
```

These queries return zero rows for the hidden dictionary names.

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
```

This slice only changes name classification and diagnostics after parsing.

## Semantics

### Name Classification

The covered names are not added to the queryable mysql-system-table definition
registry. They do not receive column metadata, index metadata, table status
rows, direct placeholder rows, or catalog-backed rows.

Instead, MyLite classifies them as hidden MySQL dictionary/system names when:

- a `SELECT` has a single table source that resolves to `mysql.<name>`;
- the selected schema is `mysql` and an unqualified `SELECT` source uses one
  of the names;
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `SHOW INDEXES`, or `SHOW KEYS` targets `mysql.<name>` or the selected
  `mysql` schema with an unqualified hidden name.

Once classified, MyLite raises `3554 / HY000` before trying to resolve a MyLite
catalog table or a supported mysql-system-table definition.

### Directory Surfaces

The hidden names remain excluded from MyLite's built-in `mysql` table
directory. They therefore do not appear in:

- `SHOW TABLES FROM mysql`;
- `SHOW FULL TABLES FROM mysql`;
- `INFORMATION_SCHEMA.TABLES`;
- `SHOW TABLE STATUS FROM mysql`.

This matches the observed MySQL 8.4.9 runtime and keeps the existing
`baseline-built-in-schema-table-directory` row counts stable.

### INFORMATION_SCHEMA Interactions

This slice does not add or remove any `INFORMATION_SCHEMA` table
implementation. Existing supported `INFORMATION_SCHEMA` views continue to be
the public metadata access path for schema, table, column, index, routine,
trigger, charset, collation, check-constraint, and tablespace metadata.

### Writes

Writes to built-in schemas already fail through the existing system-schema
write guard. This slice is read/metadata-statement focused and does not add
new DDL or DML mutation paths for hidden dictionary tables.

## Diagnostics And Limits

- `SELECT`, `SHOW COLUMNS`, `DESCRIBE`, and `SHOW INDEX` for covered hidden
  names return `3554 / HY000`.
- Unknown non-hidden `mysql` names continue to use the existing unsupported or
  table-not-found diagnostics for their statement class.
- No data dictionary storage is added.
- No `SHOW CREATE TABLE mysql.<hidden-name>` support is added.
- No debug-build dictionary access mode is added.
- No physical SQLite tables, virtual tables, or SQLite fork hooks are added.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: adds hidden dictionary table name classification and
  MySQL-shaped diagnostics.
- Metadata: no new metadata rows are exposed.
- Catalog/storage/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT COUNT(*) FROM mysql.catalogs;
SELECT COUNT(*) FROM mysql.schemata;
SHOW COLUMNS FROM mysql.catalogs;
SHOW COLUMNS FROM mysql.innodb_ddl_log;
SHOW INDEX FROM mysql.schemata;
DESC mysql.tables;
SELECT COUNT(*)
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('catalogs','schemata','tables','innodb_ddl_log');
SHOW FULL TABLES FROM mysql
 WHERE Tables_in_mysql IN ('catalogs','schemata','tables','innodb_ddl_log');
USE mysql;
SELECT COUNT(*) FROM schemata;
```

The target runtime returned `3554 / HY000` for the direct and metadata access
forms, returned zero `INFORMATION_SCHEMA.TABLES` rows, returned zero
`SHOW FULL TABLES` rows, and accepted `USE mysql` before rejecting the
unqualified dictionary-table read.

## Test Plan

- Add a MySQL expectation script that verifies:
  - all covered hidden names reject direct `SELECT COUNT(*)` with `3554`;
  - representative metadata statements reject with `3554`;
  - `innodb_ddl_log` keeps the observed "system table" wording;
  - hidden names are absent from `SHOW FULL TABLES` and
    `INFORMATION_SCHEMA.TABLES`.
- Add focused C runtime coverage for:
  - qualified and unqualified `SELECT` diagnostics;
  - `SHOW COLUMNS`, `DESCRIBE`, and `SHOW INDEX` diagnostics;
  - hidden names remaining absent from directory metadata.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_data_dictionary_table_diagnostics_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_data_dictionary_table_diagnostics_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_data_dictionary_table_diagnostics_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_data_dictionary_table_diagnostics$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
