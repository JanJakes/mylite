# Baseline sys.version View

This slice adds a MySQL-shaped `sys.version` baseline. MyLite exposes the view
as a read-only synthetic two-column row with MySQL 8.4.9-compatible metadata for
direct reads, selected-schema reads, `SHOW COLUMNS`, `SHOW FULL COLUMNS`,
`DESCRIBE`, and `INFORMATION_SCHEMA.COLUMNS`. Existing built-in schema
directory support already exposes the `INFORMATION_SCHEMA.TABLES` and
`SHOW TABLE STATUS` view-status rows.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.version`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-version.html>
- MySQL 8.4 Reference Manual, using the `sys` schema:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-usage.html>
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_version_view_expectations.sh`.

The MySQL manual describes `sys.version` as a deprecated sys-schema view that
provides the current sys schema version and MySQL server version. MyLite exposes
the compatibility row because applications and diagnostics may still probe it.

## Supported Behavior

Supported direct reads:

```sql
SELECT sys_version, mysql_version FROM sys.version;

USE sys;
SELECT * FROM version;
```

The view returns one row:

| sys_version | mysql_version |
| --- | --- |
| `2.1.3` | `8.4.9` |

`mysql_version` follows MyLite's current MySQL compatibility server-version
constant. Reads report zero affected rows and preserve MySQL's `ROW_COUNT() =
-1` post-select behavior.

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior are inherited from the existing synthetic system-table query engine.

## Column Metadata

`sys.version` has two columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `sys_version` | `varchar(5)` | `NO` | `` | empty string | `` | `utf8mb4_0900_ai_ci` |
| `mysql_version` | `varchar(5)` | `NO` | `` | empty string | `` | `utf8mb3_general_ci` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, and `DESCRIBE` expose the same shape.
`SHOW FULL COLUMNS` reports fixed privileges
`select,insert,update,references` and empty comments. `INFORMATION_SCHEMA.COLUMNS`
reports MySQL 8.4.9 ordinal positions, empty-string defaults, nullability,
character lengths, character sets, collations, column types, privileges, empty
comments, and empty generation expressions.

## Indexes And Constraints

`sys.version` has no index, primary key, unique key, foreign key, or check
metadata. `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
and `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- the full sys schema view catalog;
- persisted view descriptors or a physical SQLite view;
- privilege filtering, definer enforcement, or SQL SECURITY behavior;
- mutable sys schema installation state;
- broader sys helper functions that may depend on sys schema views.

The follow-up `baseline-sys-version-view-definition` slice adds synthetic
`INFORMATION_SCHEMA.VIEWS`, `SHOW CREATE VIEW`, and `SHOW CREATE TABLE`
metadata for this view without adding persisted sys view descriptors.

Writes to `sys.version` remain blocked by the built-in schema write guard.

## Parser And Grammar

No new grammar is required. The feature uses existing qualified and
selected-schema table references, existing `SHOW COLUMNS`, `SHOW INDEX`, and
`INFORMATION_SCHEMA` query support.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the existing synthetic system-table descriptor
  table with a `schema_name = 'sys'`, `name = 'version'` view entry.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and result builder for direct reads.
- SHOW metadata: reuses the existing synthetic system-table column/index
  rendering paths after resolving the explicit `sys.version` descriptor.
- Information schema: existing synthetic `COLUMNS`, `STATISTICS`,
  `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `TABLE_CONSTRAINTS_EXTENSIONS` loops consume the descriptor.
- Storage/SQLite: unchanged. No physical view or SQLite fork hook is required.

## Performance

The view has one static row and two columns. Direct reads materialize a bounded
row set before applying the existing metadata-query filter and projection logic.

## Tests

MySQL 8.4.9 expectation coverage:

- direct qualified and selected-schema reads;
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, and empty `SHOW INDEX`;
- `INFORMATION_SCHEMA.COLUMNS`, `STATISTICS`, and constraint metadata;
- existing `INFORMATION_SCHEMA.TABLES` / `SHOW TABLE STATUS` view-status rows.

MyLite runtime coverage:

- direct qualified and selected-schema reads;
- row-count status after a synthetic view read;
- SHOW and information-schema column metadata;
- empty index/statistics/constraint metadata.
