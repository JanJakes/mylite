# SHOW COLUMNS / SHOW FIELDS

## Scope

This feature implements the first executable `SHOW COLUMNS` metadata slice for
supported persistent MyLite base tables:

- `SHOW COLUMNS FROM tbl_name`
- `SHOW FIELDS FROM tbl_name`
- `SHOW FULL COLUMNS FROM tbl_name`
- `SHOW FULL FIELDS FROM tbl_name`
- `SHOW EXTENDED COLUMNS FROM tbl_name`
- `SHOW [EXTENDED] [FULL] {COLUMNS | FIELDS} {FROM | IN} tbl_name {FROM | IN} db_name`
- `SHOW [EXTENDED] [FULL] {COLUMNS | FIELDS} {FROM | IN} db_name.tbl_name`
- `SHOW [EXTENDED] [FULL] {COLUMNS | FIELDS} {FROM | IN} tbl_name LIKE 'pattern'`
- `SHOW [EXTENDED] [FULL] {COLUMNS | FIELDS} {FROM | IN} tbl_name WHERE expr`

The slice reads MyLite's table and column catalogs and returns MySQL-compatible
result column names and ordering for persistent base tables. It includes exact
`FULL` result-set shape, `FIELDS` synonym support, selected-schema and
explicit-schema resolution, column-name `LIKE` filtering, and deterministic
diagnostics for missing schema or table context.

Deferred surfaces:

- execution of `SHOW COLUMNS ... WHERE expr`
- hidden storage-engine columns for `EXTENDED`
- user views, until `CREATE VIEW` metadata exists
- temporary tables and temporary-table shadowing
- privilege filtering beyond MyLite's current stored privilege string
- detailed information-schema system-view column descriptions
- generated invisible primary key settings and hidden InnoDB-style columns
- metadata locks, protocol status counters, and performance-schema accounting

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW COLUMNS` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html
- MySQL 8.4 Reference Manual, `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- MySQL 8.4 Reference Manual, Extensions to `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- Runtime observations verified against `mylite-mysql-849`, MySQL `8.4.9`.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW COLUMNS FROM meta` with no selected database and unqualified table name | Error `1046`, SQLSTATE `3D000`, message `No database selected`. |
| `SHOW COLUMNS FROM meta FROM missing_schema` | Error `1049`, SQLSTATE `42000`, message `Unknown database 'missing_schema'`. |
| `SHOW COLUMNS FROM missing_table` in schema `mylite_show_columns_errors` | Error `1146`, SQLSTATE `42S02`, message `Table 'mylite_show_columns_errors.missing_table' doesn't exist`. |
| `SHOW COLUMNS FROM meta` | Columns `Field`, `Type`, `Null`, `Key`, `Default`, `Extra`. |
| `SHOW FULL COLUMNS FROM meta` | Columns `Field`, `Type`, `Collation`, `Null`, `Key`, `Default`, `Extra`, `Privileges`, `Comment`. |
| `SHOW FIELDS FROM meta` | Same result shape and rows as `SHOW COLUMNS FROM meta`. |
| `SHOW FULL FIELDS FROM meta FROM mylite_show_columns_probe` | Same result shape and rows as `SHOW FULL COLUMNS`. |
| `SHOW COLUMNS FROM mylite_show_columns_probe.meta` | Equivalent to `SHOW COLUMNS FROM meta FROM mylite_show_columns_probe`. |
| `SHOW COLUMNS FROM meta LIKE 'name'` | Returns column `name`. |
| `SHOW COLUMNS FROM meta LIKE 'Name'` | Also returns column `name` on the verified runtime. |
| `SHOW COLUMNS FROM meta LIKE 'a\_%'` | Backslash escapes `_`, so only literal-underscore names such as `a_1` match. |
| `SHOW COLUMNS FROM meta WHERE Field = 'name'` | Filters rows by the displayed `Field` column. |
| `SHOW EXTENDED COLUMNS FROM meta` | Includes internal hidden storage-engine columns in MySQL; MyLite currently has no equivalent hidden-column catalog. |
| `SHOW COLUMNS FROM gen_cols` with stored and virtual generated columns | Reports generated columns with `Extra` values `STORED GENERATED` and `VIRTUAL GENERATED`; omitted storage defaults to virtual. |
| `SHOW FULL COLUMNS FROM information_schema.COLUMNS LIKE 'COLUMN_NAME'` | Returns `COLUMN_NAME` with the `FULL` result shape; MyLite defers system-view column descriptions for this slice. |
| `SHOW COLUMNS FROM missing_info FROM information_schema` | Error `1109`, SQLSTATE `42S02`, message `Unknown table 'MISSING_INFO' in information_schema`. |
| `CREATE TABLE columns (fields INT, columns INT, extended INT, full INT)` | `COLUMNS`, `FIELDS`, `EXTENDED`, and `FULL` are usable as unquoted identifiers in table and column contexts. |

On the Linux MySQL runtime used for verification, `lower_case_table_names = 0`.
Table names are case-sensitive, but `SHOW COLUMNS ... LIKE` matched ASCII
column-name case-insensitively under the default connection collation.

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_columns_statement.

show_columns_statement ::= SHOW opt_extended opt_full show_columns_keyword
                           show_columns_source opt_show_columns_filter.

show_columns_keyword ::= COLUMNS.
show_columns_keyword ::= FIELDS.

show_columns_source ::= FROM table_name opt_show_columns_schema.
show_columns_source ::= IN table_name opt_show_columns_schema.

opt_show_columns_schema ::= .
opt_show_columns_schema ::= FROM identifier.
opt_show_columns_schema ::= IN identifier.

opt_show_columns_filter ::= .
opt_show_columns_filter ::= LIKE STRING.
opt_show_columns_filter ::= where_clause.
```

`COLUMNS`, `FIELDS`, `EXTENDED`, and `FULL` must remain available as
nonreserved identifiers outside the `SHOW COLUMNS` production.

## AST

Add a `show_columns_statement` AST node with:

- a boolean `extended` marker when `EXTENDED` is present
- a boolean `full` marker when `FULL` is present
- a table-name child, which may be schema-qualified
- an optional explicit schema-name child
- an optional string-literal `LIKE` pattern child or `WHERE` clause child

The statement must preserve the source span from `SHOW` through the last token.

## Runtime Semantics

Target resolution:

- If `{FROM | IN} db_name` follows the table name, use that schema.
- Otherwise, if `tbl_name` is schema-qualified, use its left schema component
  and right table component.
- Otherwise, use the session selected schema.
- If no target schema is available, return `No database selected`.
- If the target schema is missing, return `Unknown database '<schema>'`.
- If the target table is missing, return `Table '<schema>.<table>' doesn't exist`.

Rows:

- User schemas are backed by `__mylite_column_catalog`.
- Rows are returned in `ordinal_position` order.
- `SHOW COLUMNS` and `SHOW FIELDS` return:
  `Field`, `Type`, `Null`, `Key`, `Default`, `Extra`.
- `SHOW FULL COLUMNS` and `SHOW FULL FIELDS` return:
  `Field`, `Type`, `Collation`, `Null`, `Key`, `Default`, `Extra`,
  `Privileges`, `Comment`.
- `Default` is SQL `NULL` when the catalog stores no default.
- `Key` uses the catalog's highest-priority key marker: `PRI`, `UNI`, `MUL`,
  or empty string.
- `Extra` uses the catalog's stored extra marker, including `auto_increment`,
  `on update CURRENT_TIMESTAMP`, generated-default markers, generated-column
  storage markers, and `INVISIBLE` where present.
- `Privileges` uses MyLite's stored column privilege string for now.
- `SHOW EXTENDED COLUMNS` is accepted and currently returns the same user
  columns as `SHOW COLUMNS`; MyLite has no hidden storage-engine column catalog
  for this slice.
- `SHOW COLUMNS ... WHERE expr` is evaluated over displayed result columns.
  `FULL`-only columns such as `Collation`, `Privileges`, and `Comment` are only
  valid when `FULL` is present.
- Unknown displayed-column identifiers return MySQL error `1054`.
- Broader SHOW `WHERE` expressions remain deferred to the shared filter.
- `information_schema` table-column introspection is parsed, validates the
  system table name where possible, and returns a deterministic unsupported
  diagnostic until MyLite has system-view column descriptions.
- Missing `information_schema` table names are reported with MySQL's uppercase
  table-name display in the diagnostic.

LIKE filtering:

- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte for SHOW-pattern purposes.
- Matching uses SQLite `LIKE ... ESCAPE '\'`, which matches the verified
  case-insensitive ASCII behavior for column names under MyLite's current
  default connection model.

Warnings and affected rows:

- Successful `SHOW COLUMNS` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only SQLite-backed result.

## Storage And Performance

This feature is read-only. It must not mutate schema, table, column, or index
catalog rows. Runtime execution should lower to a compact SQLite read statement
over `__mylite_column_catalog`, with no per-row C-side materialization for base
tables.

## Tests

Parser coverage:

- `SHOW COLUMNS FROM t`
- `SHOW FIELDS IN t`
- `SHOW FULL COLUMNS FROM t`
- `SHOW FULL FIELDS FROM t FROM db`
- `SHOW EXTENDED COLUMNS FROM t`
- `SHOW EXTENDED FULL FIELDS IN db.t LIKE 'a%'`
- `SHOW COLUMNS FROM db.t`
- `SHOW COLUMNS FROM t LIKE 'name'`
- `SHOW COLUMNS FROM t WHERE Field = 'name'`
- generated-column metadata for `GENERATED ALWAYS AS (...) STORED`,
  `GENERATED ALWAYS AS (...) VIRTUAL`, and `AS (...)` default-virtual forms
- `COLUMNS`, `FIELDS`, `EXTENDED`, and `FULL` as unquoted identifiers in table
  and column definitions
- syntax rejection for missing table name, non-string `LIKE`, combined `LIKE`
  plus `WHERE`, and duplicate `FULL`

Runtime coverage:

- selected-schema listing with exact non-`FULL` column names and rows
- `FIELDS` synonym
- `FULL` result shape, collation, privileges, and comments
- schema-qualified listing via `FROM` and `IN`
- `db.table` target resolution
- `EXTENDED` accepted as current no-op over MyLite's user-column catalog
- generated-column `Extra` values through `SHOW COLUMNS`, `SHOW FULL COLUMNS`,
  and `DESCRIBE`
- parsed `WHERE` returns a deterministic unsupported diagnostic
- `LIKE` wildcard filtering and case behavior
- escaped `_` in `LIKE`
- empty result with stable metadata
- no selected schema diagnostic
- missing schema diagnostic
- missing table diagnostic
- keyword interaction for table and column names using `columns`, `fields`,
  `extended`, and `full`
