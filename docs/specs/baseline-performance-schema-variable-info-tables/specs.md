# Baseline Performance Schema variable info tables

## Scope

This baseline covers two MySQL 8.4.9 `performance_schema` system-variable
metadata tables:

- `performance_schema.persisted_variables`
- `performance_schema.variables_info`

The implemented surface includes MySQL-shaped table descriptors, column
metadata, primary-key metadata where MySQL exposes it, table-status metadata,
read-only query results, selected-schema resolution, and built-in-schema write
protection.

## Compatibility authority

The specification is based on the MySQL 8.4 Reference Manual pages for
Performance Schema system variable tables:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-system-variable-tables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-persisted-variables-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-variables-info-table.html>

Runtime metadata was verified against a MySQL 8.4.9 container named
`mylite-mysql-849` with:

```sql
SELECT VERSION();
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
       COLUMN_TYPE, COLUMN_KEY
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('persisted_variables', 'variables_info')
 ORDER BY TABLE_NAME, ORDINAL_POSITION;
SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
       COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('persisted_variables', 'variables_info')
 ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('persisted_variables', 'variables_info')
 ORDER BY TABLE_NAME, CONSTRAINT_NAME;
SELECT COUNT(*) FROM performance_schema.persisted_variables;
SELECT COUNT(*) FROM performance_schema.variables_info;
SELECT VARIABLE_NAME, VARIABLE_SOURCE, VARIABLE_PATH, MIN_VALUE, MAX_VALUE,
       SET_TIME, SET_USER, SET_HOST
  FROM performance_schema.variables_info
 WHERE VARIABLE_NAME IN ('autocommit', 'time_zone', 'sql_mode',
                         'performance_schema', 'max_connections', 'version')
 ORDER BY VARIABLE_NAME;
```

Observed MySQL 8.4.9 metadata:

- `persisted_variables` has `VARIABLE_NAME varchar(64) NOT NULL` and
  `VARIABLE_VALUE varchar(1024) NULL`, using `utf8mb4_0900_ai_ci`.
- `persisted_variables` has a HASH primary key on `VARIABLE_NAME`.
- A fresh target runtime has no persisted variable rows.
- `variables_info` has eight columns: `VARIABLE_NAME`,
  `VARIABLE_SOURCE`, `VARIABLE_PATH`, `MIN_VALUE`, `MAX_VALUE`, `SET_TIME`,
  `SET_USER`, and `SET_HOST`.
- `VARIABLE_SOURCE` is an enum containing `COMPILED`, `GLOBAL`, `SERVER`,
  `EXPLICIT`, `EXTRA`, `USER`, `LOGIN`, `COMMAND_LINE`, `PERSISTED`, and
  `DYNAMIC`.
- `SET_TIME` is `timestamp(6)`, `SET_USER` is `char(32) utf8mb4_bin`, and
  `SET_HOST` is `char(255) ascii_general_ci`.
- `variables_info` has no indexes or table constraints.
- Both tables are `BASE TABLE` objects with `ENGINE='PERFORMANCE_SCHEMA'`,
  `ROW_FORMAT='Dynamic'`, `AUTO_INCREMENT=NULL`, and
  `TABLE_COLLATION='utf8mb4_0900_ai_ci'`.

## MyLite behavior

MyLite exposes both tables as read-only synthetic Performance Schema tables.

`persisted_variables` is an empty table. MyLite does not store server-global
system-variable persistence in a `mysqld-auto.cnf` equivalent and does not
implement `SET PERSIST` or `PERSIST_ONLY`, so no rows are generated.

`variables_info` emits one row for each system variable descriptor supported by
MyLite's registry. For this baseline, each row reports:

- `VARIABLE_NAME` from the descriptor name.
- `VARIABLE_SOURCE = 'COMPILED'`.
- `VARIABLE_PATH = ''`.
- `MIN_VALUE = '0'` and `MAX_VALUE = '0'`.
- `SET_TIME = NULL`.
- `SET_USER = ''` and `SET_HOST = ''`.

This mirrors MySQL's table shape and provides useful discoverability for
applications that join `variables_info` to `global_variables` or
`session_variables`, while keeping the value-source details honest for an
embedded runtime without server startup option files or persisted globals.

## Explicit gaps

- No `mysqld-auto.cnf` storage, `SET PERSIST`, `PERSIST_ONLY`, or persisted
  global variable load path is implemented.
- `variables_info` does not yet track the true source that most recently set a
  variable. Runtime `SET` statements do not change `VARIABLE_SOURCE`,
  `SET_TIME`, `SET_USER`, or `SET_HOST`.
- Exact numeric min/max ranges are not yet stored per variable; all supported
  rows use the documented placeholder range `0..0`.
- Sensitive-variable privilege filtering is not modeled separately because
  MyLite currently exposes one embedded account identity and no persisted
  sensitive values.
- The row count reflects MyLite's supported variable registry rather than every
  optional MySQL plugin or build-time variable.

## Runtime and storage design

This is a MyLite metadata implementation. It uses the existing built-in table
descriptor framework and system-variable registry. No SQLite user table, file
format change, public SQLite extension API, or targeted SQLite fork hook is
required.

Rows are synthesized at query time. There is no persistent storage and no new
dependency.

No new SQL grammar is required. Existing metadata query parsing covers:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
qualified_table_name ::= ident DOT ident.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
```

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_variable_info_tables_expectations.sh`
  verifies MySQL 8.4.9 column, index, constraint, table, and representative row
  expectations.
- `packages/libmylite/tests/runtime_performance_schema_variable_info_tables_test.c`
  verifies MyLite query rows, row counts, metadata surfaces, selected-schema
  resolution, and write protection.
