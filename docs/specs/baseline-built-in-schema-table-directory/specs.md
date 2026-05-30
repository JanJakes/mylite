# Baseline Built-In Schema Table Directory

## Summary

MyLite exposes a metadata-only table directory for MySQL's built-in schemas:

- `information_schema`
- `mysql`
- `performance_schema`
- `sys`

The previous built-in schema catalog slice made the schema names selectable and
visible. This slice adds table-level directory rows for introspection statements
that applications use to discover server metadata. The rows are synthetic
runtime metadata, not durable MyLite catalog descriptors and not SQLite tables.

This slice covers:

- `INFORMATION_SCHEMA.TABLES` rows for the MySQL 8.4.9 table names and table
  types in the four built-in schemas.
- `SHOW TABLES` and `SHOW FULL TABLES` for selected or explicitly named
  built-in schemas, including existing `LIKE` and `WHERE` filters.
- `SHOW TABLE STATUS` for selected or explicitly named built-in schemas,
  including existing `LIKE` and `WHERE` filters.

This slice does not make unsupported system tables queryable. `SELECT` from
`mysql.user`, `performance_schema.*`, `sys.*`, or unsupported
`information_schema.*` tables remains outside the slice unless that table is
already implemented by a separate MyLite feature. It also does not implement
system-table columns, system-table data rows, privileges, live storage-engine
statistics, Performance Schema instrumentation, sys view definitions, or
mutable `mysql` system tables.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `SHOW TABLES` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-tables.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_builtin_schema_table_directory_expectations.sh`.

Observed MySQL 8.4.9 directory counts for this target runtime:

| Schema | Table rows | Notes |
| --- | ---: | --- |
| `information_schema` | 78 | all `SYSTEM VIEW` |
| `mysql` | 38 | all `BASE TABLE` in this target runtime |
| `performance_schema` | 114 | all `BASE TABLE` with `ENGINE = PERFORMANCE_SCHEMA` |
| `sys` | 101 | one `BASE TABLE` (`sys_config`) and 100 `VIEW` rows |

Observed SHA-256 hashes over `TABLE_NAME|TABLE_TYPE`, ordered by table name:

| Schema | Hash |
| --- | --- |
| `information_schema` | `1feb3d9b1aaf492c7b5a41e273627bef02b30f309e4b1a89185e45edf46d346b` |
| `mysql` | `2822370c2fb092cf5f80de8bbfba03a94cb5c1bb577931810e582dd0b03eff22` |
| `performance_schema` | `a9d6480e48494356bb86c550487dfd0f56a445fb7d14c02a6b57ec1c4fe361ca` |
| `sys` | `148115307826b6c6a0c7140932785cada503b7e0fc892e886670fd93da6fcba1` |

The combined ordered hash over `TABLE_SCHEMA|TABLE_NAME|TABLE_TYPE` is
`adc4825f20567d48cffc1d402fa967b42104818a993c9265118b83c37f30c75d`.

## Syntax

No parser grammar expansion is required. Existing MyLite grammar already admits
the statement shapes used by this slice:

```lemon
cmd ::= SHOW full_opt TABLES show_tables_schema_opt show_like_or_where_opt.
cmd ::= SHOW TABLE STATUS show_table_status_schema_opt show_like_or_where_opt.
table_factor ::= qualified_name.
```

The existing `SHOW TABLES` and `SHOW TABLE STATUS` filter subset continues to
apply. This slice does not add `EXTENDED`, `ORDER BY`, `LIMIT`, arbitrary
`WHERE` expressions, or combined `LIKE ... WHERE` support.

## Semantics

### Synthetic Directory Rows

MyLite owns a fixed in-process built-in table directory. Each entry stores:

- schema name;
- table name;
- `TABLE_TYPE` / `SHOW FULL TABLES` type;
- metadata fields needed by `INFORMATION_SCHEMA.TABLES` and
  `SHOW TABLE STATUS`.

These rows are not persisted and do not receive MyLite schema ids, table ids,
physical names, descriptor versions, or catalog generations. They must not be
written to `_mylite_catalog_*` tables and must not create SQLite schema objects.

Directory rows are visible only through table-directory introspection:

- `INFORMATION_SCHEMA.TABLES`
- `SHOW TABLES`
- `SHOW FULL TABLES`
- `SHOW TABLE STATUS`

Unsupported system-table reads remain unsupported even when the table appears
in the directory. For example, listing `mysql.user` does not imply account-table
storage, privilege enforcement, or queryable `mysql.user` rows.

### INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` appends built-in directory rows before durable
MyLite catalog rows. The existing metadata query pipeline then handles
projection, filtering, ordering, limit, aggregate count, and diagnostics.

For `information_schema` rows, MyLite reports MySQL-shaped system-view metadata:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'information_schema'`
- `TABLE_TYPE = 'SYSTEM VIEW'`
- `ENGINE = NULL`
- `VERSION = '10'`
- `ROW_FORMAT = NULL`
- row and length counters use MySQL's zero-valued system-view shape
- `CREATE_TIME` is rendered from the current statement time, matching MySQL's
  non-`NULL` timestamp shape for built-in table rows without persisting server
  startup state
- `UPDATE_TIME` and `CHECK_TIME` remain SQL `NULL` in this metadata-only slice
- `TABLE_COLLATION`, `CHECKSUM`, and `AUTO_INCREMENT` are SQL `NULL`
- `CREATE_OPTIONS` and `TABLE_COMMENT` are empty strings

For `mysql`, `performance_schema`, and `sys`, MyLite reports a stable
metadata-directory shape derived from MySQL 8.4.9 table names and table types.
Engine, version, row-format, collation, and comment fields are MySQL-shaped
where they are stable enough for metadata discovery, but live row counts,
auto-increment counters, data lengths, data-free bytes, update times, and
storage-engine statistics are placeholders unless a later feature implements
the actual system table. This keeps the directory useful without pretending to
own live server internals.

The `baseline-mysql-system-stats-table-status` slice refines the supported
`mysql.innodb_table_stats` and `mysql.innodb_index_stats` rows with
MySQL-observed table-status values for row counts, average row length,
`DATA_FREE`, and non-`NULL` update-time shape. Other built-in directory rows
continue to use the generic placeholder status values from this directory
baseline until separately specified.

The `baseline-mysql-component-table` slice refines `mysql.component` with
MySQL-observed `AUTO_INCREMENT`, `DATA_FREE`, comment, create-options, and
primary-key metadata while keeping the table read-only and empty.

The `baseline-mysql-func-table` slice refines `mysql.func` with direct empty
reads plus MySQL-observed `DATA_FREE`, comment, create-options, and
primary-key metadata while keeping loadable-function execution, persistence,
and writes out of scope.

The `baseline-mysql-plugin-table` slice refines `mysql.plugin` with direct
connection-control plugin registry rows plus MySQL-observed row count,
`DATA_FREE`, comment, create-options, non-`NULL` update-time shape, and
primary-key metadata while keeping plugin loading, lifecycle, mutable plugin
registry state, and writes out of scope.

The `baseline-mysql-cost-tables` slice refines `mysql.server_cost` and
`mysql.engine_cost` with direct optimizer cost-default rows plus MySQL-observed
row counts, `DATA_FREE`, create-options, generated-column metadata,
primary-key metadata, and table-status fields while keeping mutable cost
overrides, optimizer cost reload, and writes out of scope.

The `baseline-mysql-servers-table` slice refines `mysql.servers` with direct
empty reads plus MySQL-observed `DATA_FREE`, comment, create-options, and
primary-key metadata while keeping server-definition DDL, FEDERATED
connections, persistence, and writes out of scope.

The `baseline-mysql-gtid-executed-table` slice refines `mysql.gtid_executed`
with direct empty reads plus MySQL-observed `DATA_FREE`, collation,
create-options, empty comment, column comments, and composite primary-key
metadata while keeping GTID persistence, compression, binary logging,
replication state, and writes out of scope.

The `baseline-mysql-log-tables` slice refines `mysql.general_log` and
`mysql.slow_log` with direct empty reads plus MySQL-observed CSV engine status,
`Rows = 2` status estimates, zero data/index/free lengths, empty create
options, table comments, column metadata, and no-index/no-constraint metadata
while keeping log-row storage, log writing, log routing, rotation, and writes
out of scope.

The `baseline-mysql-help-tables` slice refines the four `mysql.help_*` tables
with direct empty placeholder reads plus MySQL-observed InnoDB status fields,
comments, column metadata, primary-key metadata, and unique `name` metadata
where present while keeping bundled help content, HELP statement execution,
help-table initialization, upgrades, and writes out of scope.

The `baseline-mysql-time-zone-tables` slice refines the five
`mysql.time_zone*` tables with direct empty placeholder reads plus
MySQL-observed InnoDB status fields, comments, column metadata, and
primary-key metadata while keeping zoneinfo row loading, named time-zone
conversion, leap-second handling, and writes out of scope.

### SHOW TABLES and SHOW FULL TABLES

When the selected schema or explicit `FROM` / `IN` schema is built in,
`SHOW TABLES` reads the built-in directory instead of the durable MyLite
catalog. User schemas continue to use durable table descriptors.

`SHOW TABLES` returns the table-name column only. `SHOW FULL TABLES` returns the
table-name column and `Table_type`, using MySQL's type text from the directory:

- `SYSTEM VIEW`
- `BASE TABLE`
- `VIEW`

Existing `LIKE` and admitted `WHERE` filters apply to the synthetic rows with
the same result-column labels used for user schemas, such as
`Tables_in_sys` and `Table_type`.

Directory rows are ordered by the static MySQL 8.4.9 name order captured in the
feature. The order is case-sensitive only to the extent visible in the recorded
names; no collation engine is introduced.

### SHOW TABLE STATUS

When the selected schema or explicit `FROM` / `IN` schema is built in,
`SHOW TABLE STATUS` reads the built-in directory instead of durable MyLite
catalog descriptors. It returns the existing MyLite `SHOW TABLE STATUS` column
labels.

System views and ordinary views expose MySQL-shaped view/system-view storage
fields with `NULL` storage-engine values except where MySQL uses `VERSION = 10`
for `information_schema` system views. Base-table rows expose stable directory
fields for engine, version, row format, collation, create options, and comment.
Live statistics remain placeholders unless explicitly recorded as stable for
the directory row.

`Create_time` in `SHOW TABLE STATUS` is rendered from the current statement
time for built-in directory rows that MySQL reports with a server-created
timestamp. This gives callers a non-`NULL` timestamp shape without creating a
durable catalog timestamp or depending on host-server startup time.
`Update_time` and `Check_time` remain SQL `NULL` except where later slices
document a non-`NULL` built-in-table status shape, such as the supported
`mysql.innodb_table_stats` and `mysql.innodb_index_stats` rows.

Existing `LIKE` and admitted `WHERE` filters apply to the synthetic rows. The
filter uses the displayed status values, not SQLite metadata.

## Diagnostics

Supported diagnostics:

- missing selected schema: existing no-database-selected diagnostic;
- unknown explicit user schema: existing unknown-database diagnostic;
- reserved `_mylite_*` explicit schema names: existing reserved-name
  diagnostic;
- unsupported `SHOW` filter syntax: existing parser/runtime diagnostics;
- allocation failure: existing `MYLITE_NOMEM` diagnostics;
- direct reads from unsupported system tables: existing information-schema
  unknown-table diagnostics for unsupported `information_schema` tables and
  existing user-table resolution diagnostics for unsupported `mysql`,
  `performance_schema`, or `sys` tables.

This slice does not add new public API misuse diagnostics.

## Architecture

- Public API: unchanged. The feature is visible only through existing
  `mylite_execute()` and result APIs.
- Parser/AST: unchanged; existing nodes already represent `SHOW TABLES`,
  `SHOW TABLE STATUS`, and `INFORMATION_SCHEMA.TABLES` reads.
- Statement context: reused for statement-time `SHOW TABLE STATUS` timestamp
  rendering through the existing session active statement time.
- Runtime/analyzer: owns built-in directory lookup, SHOW resolution, synthetic
  `INFORMATION_SCHEMA.TABLES` rows, filters, and diagnostics.
- Catalog module: unchanged. Built-in table rows are not descriptors and do not
  participate in `mylite_catalog_for_each_table_in_schema()`.
- Result builder: unchanged; synthetic rows are appended through existing
  result APIs.
- Storage/VFS/SQLite: unchanged. No SQLite table, view, trigger, index, or fork
  hook is required.

## Performance

The built-in directory has 331 rows. `INFORMATION_SCHEMA.TABLES` currently
materializes metadata rows before filtering, matching the existing metadata
query architecture. This is small and bounded. `SHOW TABLES` and
`SHOW TABLE STATUS` stream the static directory rows directly into the result
after applying filters. No user data rows or physical SQLite system tables are
scanned.

## Tests

MySQL 8.4.9 expectation coverage:

- built-in schema table counts and ordered hashes;
- representative `INFORMATION_SCHEMA.TABLES` rows for `information_schema`,
  `mysql`, `performance_schema`, and `sys`;
- `SHOW FULL TABLES FROM` each built-in schema for representative names and
  table types;
- `SHOW TABLE STATUS FROM` each built-in schema for representative names and
  status shapes.

MyLite C coverage:

- `INFORMATION_SCHEMA.TABLES` counts for all four built-in schemas;
- representative row values for `information_schema.TABLES`, `mysql.user`,
  `performance_schema.setup_actors`, `sys.sys_config`, and `sys.version`;
- `SHOW TABLES` and `SHOW FULL TABLES` from selected and explicit built-in
  schemas, including `LIKE` and admitted `WHERE` filters;
- `SHOW TABLE STATUS` from selected and explicit built-in schemas, including
  representative base table, system view, and view rows;
- user-schema `SHOW TABLES`, `SHOW FULL TABLES`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES` behavior remains descriptor-driven;
- unsupported direct reads from unimplemented system tables remain rejected and
  do not create catalog or SQLite objects.
