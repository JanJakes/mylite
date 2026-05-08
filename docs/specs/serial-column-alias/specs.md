# SERIAL column aliases

## Scope

This feature adds executable `CREATE TABLE` support for the MySQL `SERIAL`
column-type alias and the `SERIAL DEFAULT VALUE` integer-column attribute:

- `col SERIAL`
- `col integer_type SERIAL DEFAULT VALUE`
- catalog metadata for `INFORMATION_SCHEMA.COLUMNS` and `STATISTICS`
- `SHOW COLUMNS` / `DESCRIBE` metadata through the shared column catalog
- `SHOW CREATE TABLE` normalization
- `AUTO_INCREMENT` generation for omitted values

The feature is limited to ordinary supported base tables. Broader
`AUTO_INCREMENT` diagnostics, nullable auto-increment insertion edge cases,
`ALTER TABLE ... MODIFY/CHANGE` re-normalization, and unsupported integer
families remain tied to the existing auto-increment and ALTER-table follow-up
work.

## Sources

- MySQL 8.4 Reference Manual, Numeric Data Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

`SERIAL` is a type alias. In a `CREATE TABLE` column definition it behaves as a
`BIGINT UNSIGNED` column with `NOT NULL`, `AUTO_INCREMENT`, and an inline unique
key. `SHOW CREATE TABLE` normalizes the column to `bigint unsigned NOT NULL
AUTO_INCREMENT` and renders a `UNIQUE KEY` named after the column. `SHOW
COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report `DATA_TYPE='bigint'`,
`COLUMN_TYPE='bigint unsigned'`, `IS_NULLABLE='NO'`, and
`EXTRA='auto_increment'`.

`SERIAL DEFAULT VALUE` is a column attribute for integer column definitions. It
adds `NOT NULL`, `AUTO_INCREMENT`, and an inline unique key without changing the
integer family or signedness. For example, `id INT SERIAL DEFAULT VALUE`
normalizes to an `int NOT NULL AUTO_INCREMENT` column plus a unique key.

The alias effects are order-sensitive with other column attributes. A later
`NULL` attribute can make the column nullable, while an earlier `NULL` is
overridden by the alias attribute. MyLite preserves the same parse and copy
ordering, but nullable auto-increment insert semantics are outside this slice.

`id SERIAL DEFAULT VALUE` is not a valid composition: after `SERIAL` has been
used as the column type, `DEFAULT VALUE` is rejected by MySQL syntax.

When a table has no explicit primary key, MySQL reports the first non-null
single-column unique key as `COLUMN_KEY='PRI'` / `SHOW COLUMNS Key='PRI'` even
though `SHOW CREATE TABLE` still renders it as `UNIQUE KEY`. MyLite mirrors this
catalog-display behavior for new `CREATE TABLE` metadata.

## MyLite behavior

### Parser and AST

MyLite accepts `SERIAL` as a column type and records a dedicated AST column type
so the runtime can apply the alias before later column attributes are copied.

MyLite accepts `SERIAL DEFAULT VALUE` as a column attribute after an integer
type and records a dedicated AST column attribute. This keeps the attribute's
position in the source order, which is necessary for MySQL-compatible
interaction with later `NULL` / `NOT NULL` attributes.

Independently authored Lemon-shape grammar:

```lemon
column_type ::= serial_column_type.
serial_column_type ::= SERIAL.

column_attribute ::= SERIAL DEFAULT VALUE.
```

### Runtime

For `SERIAL` type columns, MyLite expands the copied column model to:

- AST descriptor type `BIGINT`
- unsigned type attributes
- not nullable
- auto-increment
- inline unique key

For `SERIAL DEFAULT VALUE` column attributes, MyLite expands the copied column
model to:

- not nullable
- auto-increment
- inline unique key

The normal descriptor and catalog paths then produce the same metadata as the
existing supported integer, auto-increment, and index features. No separate
storage representation is introduced; `SERIAL` uses the existing
`BIGINT UNSIGNED` auto-increment storage path.

### Metadata and display

`INFORMATION_SCHEMA.COLUMNS`, `SHOW COLUMNS`, and table-backed result metadata
use the normalized descriptor. `INFORMATION_SCHEMA.STATISTICS` receives the
generated single-column unique index. `SHOW CREATE TABLE` renders the normalized
column type and the generated unique key rather than the original alias text,
matching MySQL.

For new create-table metadata, the first non-null single-column unique key is
displayed as `PRI` only when the table has no explicit primary key. Other unique
columns continue to display as `UNI`.

## Tests

Runtime expectations were verified against MySQL 8.4.9 for:

- `CREATE TABLE serial_type (id SERIAL, note INT)`
- `CREATE TABLE serial_attr (id INT SERIAL DEFAULT VALUE, note INT)`
- `SHOW CREATE TABLE`
- `SHOW COLUMNS`
- `INFORMATION_SCHEMA.COLUMNS`
- `INFORMATION_SCHEMA.STATISTICS`
- omitted-column inserts that generate `AUTO_INCREMENT` values
- syntax rejection for `id SERIAL DEFAULT VALUE`

MyLite coverage includes parser AST assertions and runtime assertions for the
same metadata and insert-generation paths.
