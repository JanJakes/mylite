# Baseline sys.schema_object_overview View

This slice adds a MySQL-shaped `sys.schema_object_overview` baseline. MyLite
exposes the sys view as a read-only synthetic catalog summary over the object
metadata surfaces currently owned by MyLite.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_object_overview`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-object-overview.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.VIEWS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-views-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_object_overview_view_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, user-schema base table, view, and index rows,
selected-schema access, view definition metadata, and dependency metadata.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.schema_object_overview;

USE sys;
SELECT db, object_type, count FROM schema_object_overview;
```

The view returns object-count rows grouped by schema name and object type. Rows
are sorted by `db`, then `object_type`, matching MySQL's default view ordering.

MyLite counts the following current metadata surfaces:

- built-in schema table-directory rows from `INFORMATION_SCHEMA.TABLES`;
- supported mysql/sys system-table BTREE index metadata rows from
  `INFORMATION_SCHEMA.STATISTICS`;
- persistent user base-table descriptors as `BASE TABLE`;
- persistent user view descriptors as `VIEW`;
- persistent user index key-part rows as `INDEX (<type>)`, using the same row
  granularity as `INFORMATION_SCHEMA.STATISTICS`;
- the existing metadata-only `sys.sys_config` triggers as `TRIGGER`.

`INDEX (BTREE)`, `INDEX (FULLTEXT)`, and `INDEX (SPATIAL)` can appear when the
underlying descriptors expose those index types.

## Column Metadata

`sys.schema_object_overview` has three columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `db` | `varchar(64)` | `NO` | empty string | `utf8mb3_bin` |
| `object_type` | `varchar(19)` | `YES` | `NULL` | `utf8mb3_bin` |
| `count` | `bigint` | `NO` | `0` | SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The view has no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows for the view itself.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes a built-in row for the view with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `TABLE_NAME = 'schema_object_overview'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=TEMPTABLE` and `DEFINER=\`mysql.sys\`@\`localhost\``. Qualified
targets render the view name as `` `sys`.`schema_object_overview` ``.
Unqualified targets resolved after `USE sys` render
`` `schema_object_overview` ``.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependency rows for
`INFORMATION_SCHEMA.EVENTS`, `ROUTINES`, `STATISTICS`, `TABLES`, and
`TRIGGERS`. `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- stored routine descriptors or sys function/procedure inventory rows;
- event descriptors;
- full Performance Schema object inventory and HASH index counts;
- privilege filtering, definer validation, or SQL SECURITY enforcement;
- physical SQLite views or persisted catalog descriptors for built-in sys
  views;
- broader sys view execution.

Writes to the view remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. The feature uses existing qualified and
selected-schema table references, `SHOW COLUMNS`, `SHOW INDEX`,
`SHOW CREATE VIEW`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` query
support.

The parser keyword mapper keeps `COUNT` as a function token when it is
immediately followed by `(`, and maps bare `COUNT` to an identifier otherwise
outside `IGNORE_SPACE` mode. That preserves aggregate parsing while allowing
MySQL-compatible unquoted reads of the view's `count` column in
`SELECT ... FROM ...` statements.

## Architecture

- Public API: unchanged.
- Parser/AST: no AST change; parser token mapping admits bare `COUNT` column
  references when the token is not an immediate function call.
- Runtime metadata: extends the synthetic system-table descriptor table with a
  `schema_name = 'sys'`, `name = 'schema_object_overview'` view entry.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds descriptor-backed aggregate rows.
- Information schema: adds `COLUMNS`, `VIEWS`, and `VIEW_TABLE_USAGE` rows
  through the existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

The view scans the built-in table-directory arrays and persistent catalog table
descriptors, loads index descriptors for base tables, aggregates a small row set,
and sorts the final groups. It performs no SQLite data-table scan.

## Tests

MySQL 8.4.9 expectation coverage:

- column, table, view, dependency, and empty index/constraint metadata;
- direct qualified and selected-schema reads;
- user-schema base-table, view, and index key-part count rows;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after a synthetic view read.

MyLite runtime coverage mirrors the supported row, metadata, and selected-schema
cases, including `DESCRIBE` and empty constraint catalogs.
