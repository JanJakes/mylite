# Baseline Performance Schema Variable And Status Tables

## Scope

MyLite exposes queryable baseline rows for these MySQL 8.4.9 Performance Schema
tables:

- `performance_schema.global_variables`
- `performance_schema.session_variables`
- `performance_schema.global_status`
- `performance_schema.session_status`

The tables are read-only built-in metadata tables. They mirror the MySQL table
shape and expose rows from MyLite's supported system-variable and status
registries. This slice does not implement live Performance Schema
instrumentation, thread/account/host/user status tables, `variables_by_thread`,
`variables_info`, `persisted_variables`, or `TRUNCATE TABLE` status reset
semantics.

## Compatibility Sources

- MySQL 8.4 Reference Manual, Performance Schema System Variable Tables:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-system-variable-tables.html
- MySQL 8.4 Reference Manual, Performance Schema Status Variable Tables:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-status-variable-tables.html
- MySQL 8.4.9 runtime probes against local container `mylite-mysql-849`.

Observed MySQL 8.4.9 behavior:

- All four tables have `VARIABLE_NAME varchar(64) NOT NULL` and
  `VARIABLE_VALUE varchar(1024) NULL`, using `utf8mb4_0900_ai_ci`.
- The primary key is `VARIABLE_NAME`.
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report the primary index type
  as `HASH`.
- `global_variables` and `session_variables` return global and current-session
  system variable values respectively.
- `global_status` and `session_status` return global and current-session status
  values respectively. `Com_%` command counters are not collected in these
  tables except the observed `Com_stmt_reprepare` row.
- `session_status` contains session-only rows such as `Compression`,
  `Compression_algorithm`, `Compression_level`, `Last_query_cost`,
  `Last_query_partial_plans`, and `Tls_sni_server_name`; those rows are absent
  from `global_status`.

Representative runtime probes:

```sql
SHOW FULL COLUMNS FROM performance_schema.global_variables;
SHOW INDEX FROM performance_schema.global_status;
SELECT VARIABLE_NAME, VARIABLE_VALUE
  FROM performance_schema.session_variables
 WHERE VARIABLE_NAME IN ('autocommit', 'performance_schema', 'sql_mode', 'time_zone')
 ORDER BY VARIABLE_NAME;
SELECT VARIABLE_NAME, VARIABLE_VALUE
  FROM performance_schema.global_status
 WHERE VARIABLE_NAME LIKE 'Com\_%'
 ORDER BY VARIABLE_NAME;
```

## MyLite Semantics

The table definitions are catalog-owned built-in system-table descriptors.
`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
`INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
`INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` use those descriptors.

Rows are synthesized at query time:

- Variable rows iterate `mylite_execution_system_variable_descriptor_*`.
  Session tables include descriptors visible to `SHOW VARIABLES`; global tables
  include descriptors visible to `SHOW GLOBAL VARIABLES`.
- Variable values use the same display formatting as `SHOW VARIABLES`, including
  session overrides such as `SET autocommit = 0` and `SET time_zone = '+02:00'`.
- Status rows iterate `mylite_execution_show_status_descriptor_*`.
  Session tables include descriptors visible to `SHOW STATUS`; global tables
  include descriptors visible to `SHOW GLOBAL STATUS`.
- Performance Schema status rows exclude `Com_%` command counters except
  `Com_stmt_reprepare`, matching observed MySQL 8.4.9 behavior for these
  tables. Session-only diagnostic rows remain session-only.

The row set is intentionally limited to MyLite's supported registries. MyLite
does not claim that the row counts match every optional server/plugin build.

## Parser And Storage

No new SQL grammar is required. Existing metadata-query parsing covers:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
qualified_table_name ::= ident DOT ident.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
```

No SQLite storage table is created. The implementation is a MyLite
wrapper/translation-layer metadata provider backed by in-memory descriptors.
No SQLite fork hook is required.

## Diagnostics And Write Access

The tables inherit built-in-schema write protection. `INSERT`, `UPDATE`,
`DELETE`, `REPLACE`, `CREATE`, `DROP`, `ALTER`, `TRUNCATE`, index DDL, and
rename attempts targeting `performance_schema` continue to return MySQL-shaped
access-denied diagnostics for built-in schemas.

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_variable_status_tables_expectations.sh`
  verifies MySQL 8.4.9 column/index/constraint/table metadata and
  representative row behavior.
- `packages/libmylite/tests/runtime_performance_schema_variable_status_tables_test.c`
  verifies MyLite query rows, session/global variable distinction, status table
  row filtering, metadata surfaces, selected-schema resolution, write
  protection, and row-count state.

## Known Gaps

- MyLite does not expose the full MySQL variable/status universe; only the
  supported registries are queryable.
- Dynamic live status accounting remains deterministic placeholder behavior for
  most status variables.
- `status_by_thread`, account/host/user summary status tables, and
  `variables_by_thread` remain unsupported.
- `TRUNCATE TABLE performance_schema.global_status` reset behavior is not
  implemented.
