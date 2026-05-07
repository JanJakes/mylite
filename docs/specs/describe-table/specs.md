# DESCRIBE / DESC Table Metadata

## Scope

This slice implements the table-description form of MySQL's `DESCRIBE`,
`DESC`, and `EXPLAIN` statements for supported persistent MyLite base tables:

- `DESCRIBE tbl_name`
- `DESC tbl_name`
- `EXPLAIN tbl_name`
- `DESCRIBE db_name.tbl_name`
- `DESC db_name.tbl_name`
- `EXPLAIN db_name.tbl_name`
- `DESCRIBE tbl_name col_name`
- `DESC tbl_name col_name`
- `EXPLAIN tbl_name col_name`
- `DESCRIBE tbl_name 'wild%'`
- `DESC tbl_name 'wild%'`
- `EXPLAIN tbl_name 'wild%'`

The result set is the non-`FULL` `SHOW COLUMNS` shape:
`Field`, `Type`, `Null`, `Key`, `Default`, `Extra`.
The C API result-column descriptors match the non-`FULL` `SHOW COLUMNS`
descriptors for the supported target-table paths.

Deferred or separate surfaces:

- `DESCRIBE`/`DESC`/`EXPLAIN` query-plan forms for `SELECT`, `TABLE`,
  `INSERT`, `REPLACE`, `UPDATE`, `DELETE`, `FOR CONNECTION`, `FORMAT`, and
  `ANALYZE` are accepted as no-op parser placeholders in the separate
  [EXPLAIN statement parser placeholders spec](../explain-statement-placeholders/specs.md);
  real plan output remains deferred
- information-schema table descriptions, matching the current `SHOW COLUMNS`
  unsupported policy for system-view column metadata
- user views and temporary tables
- privilege filtering, metadata locks, counters, and protocol status details

## Compatibility Sources

- MySQL 8.4 Reference Manual, `EXPLAIN` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/explain.html
- MySQL 8.4 Reference Manual, `SHOW COLUMNS` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html
- Runtime observations verified against Docker container `mylite-mysql-849`,
  MySQL `8.4.9`, using:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force
```

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `DESCRIBE meta` with no selected database and an unqualified table name | Error `1046`, SQLSTATE `3D000`, message `No database selected`. |
| `DESCRIBE missing_db.meta` | Error `1049`, SQLSTATE `42000`, message `Unknown database 'missing_db'`. |
| `DESCRIBE mylite_describe_probe.missing` | Error `1146`, SQLSTATE `42S02`, message `Table 'mylite_describe_probe.missing' doesn't exist`. |
| `DESCRIBE meta` | Columns `Field`, `Type`, `Null`, `Key`, `Default`, `Extra`. |
| `DESC meta` | Same result shape and rows as `DESCRIBE meta`. |
| `EXPLAIN meta` | Same table-description shape and rows as `DESCRIBE meta`. |
| `EXPLAIN meta name` | Same filtered table-description shape as `DESCRIBE meta name`. |
| `DESCRIBE meta name` | Returns only column `name`. |
| `DESCRIBE meta Name` | Returns only column `name` under the default runtime collation. |
| `DESCRIBE meta 'Name'` | Returns only column `name` under the default runtime collation. |
| ``DESCRIBE meta `name` `` | Returns only column `name`. |
| ``DESCRIBE meta `a_1` `` | Uses pattern semantics; with columns `a_1` and `ax1`, both rows matched. |
| `DESCRIBE meta 'a\_%'` | Backslash escapes `_`, so only literal-underscore names such as `a_1` match. |
| `DESCRIBE meta a_1` | Uses pattern semantics; with columns `a_1` and `ax1`, both rows matched. |
| `DESCRIBE meta 'a_'` | `_` is a wildcard and matched two-character names such as `ab` and `aX`. |
| `DESCRIBE information_schema.TABLES TABLE_NAME` | MySQL returns system-view metadata using the same six result columns; MyLite explicitly defers this surface for now. |
| `DESCRIBE information_schema.missing_info` | Error `1109`, SQLSTATE `42S02`, message `Unknown table 'MISSING_INFO' in information_schema`. |
| `DESCRIBE meta WHERE Field = 'name'` | Syntax error; the table-description form does not accept `WHERE`. |
| `EXPLAIN FORMAT=TREE SELECT 1` | Query-plan output; it is separate from the table-description form and now parses as a no-op parser placeholder in MyLite. |

For table-description result metadata, MySQL 8.4.9 reports the same descriptor
shape as non-`FULL` `SHOW COLUMNS`: `Field` as nullable `VAR_STRING(64)`,
`Type` as non-null binary `BLOB(16777215)`, `Null` as non-null
`VAR_STRING(3)`, `Key` as non-null binary enum `STRING(3)`, `Default` as
nullable binary `BLOB(65535)`, and `Extra` as nullable `VAR_STRING(256)`.

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= describe_table_statement.

describe_table_statement ::= describe_table_keyword table_name opt_describe_column_filter.

describe_table_keyword ::= DESCRIBE.
describe_table_keyword ::= DESC.
describe_table_keyword ::= EXPLAIN.

opt_describe_column_filter ::= .
opt_describe_column_filter ::= identifier.
opt_describe_column_filter ::= STRING.
```

The top-level `DESC` production must not affect `DESC` as an order direction in
`ORDER BY`, `GROUP BY`, or index key parts.

This slice intentionally keeps query-plan `EXPLAIN` outside the
`describe_table_statement` AST. `EXPLAIN FORMAT=TREE SELECT 1`,
`EXPLAIN ANALYZE SELECT ...`, and `EXPLAIN SELECT ...` are handled by
the parser-placeholder EXPLAIN slice instead.

## AST

Add a `describe_table_statement` AST node with:

- a table-name child, which may be schema-qualified
- an optional identifier or string-literal column filter child

The statement must preserve the source span from `DESCRIBE`, `DESC`, or
`EXPLAIN` through the last table/filter token.

## Runtime Semantics

Target resolution:

- If `tbl_name` is schema-qualified, use its left schema component and right
  table component.
- Otherwise, use the session selected schema.
- If no target schema is available, return `No database selected`.
- If the target schema is missing, return `Unknown database '<schema>'`.
- If the target table is missing, return
  `Table '<schema>.<table>' doesn't exist`.

Rows:

- User schemas are backed by `__mylite_column_catalog`.
- Rows are returned in `ordinal_position` order.
- The result columns are exactly:
  `Field`, `Type`, `Null`, `Key`, `Default`, `Extra`.
- `Default` is SQL `NULL` when the catalog stores no default.
- `Key` uses the catalog's stored key marker.
- `Extra` uses the catalog's stored extra marker.

Column and wildcard filtering:

- The optional argument is matched against column names using `LIKE` semantics.
- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte for wildcard matching.
- Identifiers and string literals both become patterns. This covers MySQL's
  `col_name` and `wild` behavior without needing a separate runtime branch.
- Matching uses SQLite `LIKE ... ESCAPE '\'`, which matches the verified
  case-insensitive ASCII behavior for column names under MyLite's current
  default connection model.

Unsupported surfaces:

- `information_schema` table descriptions validate known system table names and
  then return `MYLITE_UNSUPPORTED` with a deterministic diagnostic.
- Unknown `information_schema` table names return MySQL-style
  `Unknown table '<UPPERCASE_NAME>' in information_schema`.
- Query-plan `EXPLAIN` syntax is parsed by the separate placeholder slice and
  is not executed by this table-metadata runtime path.

Warnings and affected rows:

- Successful table descriptions produce no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only SQLite-backed result.

## Storage And Performance

This feature is read-only. It must not mutate schema, table, column, or index
catalog rows. Runtime execution should reuse the compact `SHOW COLUMNS` catalog
query shape and avoid per-row C-side materialization for base tables.

## Tests

Parser coverage:

- `DESCRIBE t`
- `DESC t`
- `EXPLAIN t`
- `DESCRIBE app.t`
- `DESC app.t name`
- ``DESCRIBE t `name` ``
- `DESCRIBE t 'a%'`
- `EXPLAIN t 'a\_%'`
- syntax rejection for missing table name
- syntax rejection for `DESCRIBE t WHERE Field = 'name'`
- parser-placeholder coverage for `EXPLAIN FORMAT=TREE SELECT 1` in the
  separate EXPLAIN placeholder tests
- regression coverage for `DESC` order direction in `SELECT`, DDL key parts,
  `UPDATE`, and `DELETE`

Runtime coverage:

- selected-schema `DESCRIBE` with exact column names and rows
- MySQL 8.4.9-derived result-column descriptors
- `DESC` synonym
- `EXPLAIN tbl_name` table-description synonym
- schema-qualified target resolution
- identifier column filter
- quoted identifier column filter
- string-literal pattern filter
- case-insensitive filter behavior under the current default collation
- escaped `_` in wildcard pattern
- wildcard `_` in string literal
- empty filtered result with stable metadata
- no selected schema diagnostic
- missing schema diagnostic
- missing table diagnostic
- known `information_schema` unsupported diagnostic
- unknown `information_schema` table diagnostic
