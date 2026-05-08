# BIT column type

## Scope

This feature adds the first executable `BIT` column-type slice for supported
`CREATE TABLE` paths:

- `BIT` and `BIT(n)` column definitions, where `n` is `1..64`
- catalog metadata for `INFORMATION_SCHEMA.COLUMNS`
- `SHOW COLUMNS` / `DESCRIBE` metadata through the shared column catalog
- `SHOW CREATE TABLE` normalization
- table-backed result metadata for selected `BIT` columns
- bit and hex literal inserts into `BIT` columns with fixed-width storage and
  strict/`IGNORE` overflow diagnostics

This slice does not complete all MySQL `BIT` expression semantics. Numeric and
string conversion outside covered insert paths, exact `HEX(bit_column)` display
behavior, default-expression evaluation such as `DEFAULT b'1'`, and
`ALTER TABLE ... MODIFY` / `CHANGE` conversion of existing values remain
deferred.

## Sources

- MySQL 8.4 Reference Manual, Numeric Data Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, Bit-Value Literals:
  https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html
- MySQL 8.4 Reference Manual, Hexadecimal Literals:
  https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

`BIT` is a numeric type with an optional bit precision. Omitted precision
defaults to `1`, so `BIT` and `BIT(1)` are metadata-equivalent. MySQL accepts
precisions `1..64`. It rejects `BIT(0)` with error 3013, rejects widths above
64 with error 1439, and does not accept integer signedness or character-set
attributes on `BIT`.

`SHOW CREATE TABLE` renders `BIT` as `bit(1)` and renders explicit widths as
`bit(n)`. `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE='bit'`,
`COLUMN_TYPE='bit(n)'`, `NUMERIC_PRECISION=n`, `NUMERIC_SCALE=NULL`, and null
character-length, character-set, and collation fields. `SHOW COLUMNS` renders
the normalized `bit(n)` type.

Stored `BIT` values use a fixed byte width of `ceil(n / 8)` bytes. Values are
right-aligned and left-padded with zero bytes to that storage width. For
example, inserting `b'1'` into `BIT(9)` stores two bytes `00 01`, and inserting
`b'101010101'` stores `01 55`.

Bit and hex literals assigned to `BIT` columns are width-checked against the
declared precision. In strict mode, values that need more bits than the column
allows fail with error 1406, "Data too long for column ... at row 1". Under
`INSERT IGNORE`, MySQL emits warning 1406 and clips to the largest value that
fits the declared width.

## MyLite behavior

### Parser and AST

MyLite accepts `BIT` and `BIT(n)` as column types and records a dedicated AST
column type. The parser validates the descriptor during parse-time column-type
checks, so invalid widths and disallowed attributes fail before runtime catalog
mutation.

Independently authored Lemon-shape grammar:

```lemon
column_type ::= bit_column_type.
bit_column_type ::= BIT opt_bit_precision.
opt_bit_precision ::= .
opt_bit_precision ::= column_precision.
```

### Type descriptor

The `BIT` descriptor records the MySQL-visible type separately from integer and
string families:

- `data_type='bit'`
- canonical type and `COLUMN_TYPE` of `bit(n)`
- `NUMERIC_PRECISION=n`
- no numeric scale
- no character length, character set, or collation
- storage width `ceil(n / 8)` bytes
- a dedicated `is_bit` marker for runtime decisions

The descriptor rejects signedness, display-scale, `ZEROFILL`, national,
binary/byte, and character-set/collation attributes.

### Runtime storage and metadata

Supported `CREATE TABLE` paths persist `BIT` catalog metadata and use SQLite
`BLOB` physical storage. This keeps embedded NUL bytes and leading zero padding
available to the C API while allowing MyLite to expose MySQL-compatible column
metadata.

`INFORMATION_SCHEMA.COLUMNS`, `SHOW COLUMNS`, and `SHOW CREATE TABLE` are
derived from the catalog descriptor. Table-backed result metadata reports
`MYLITE_FIELD_TYPE_BIT`, the declared bit precision as field length, and the
binary flag for selected `BIT` columns.

### Insert coercion

For covered `INSERT` value paths, bit and hex literals assigned to `BIT`
columns are decoded as binary values, checked against the declared bit width,
and stored as fixed-width big-endian bytes. Short values are left-padded with
zero bytes.

When the decoded value exceeds the declared width, strict mode reports error
1406 before mutating the row. `INSERT IGNORE` demotes the condition to warning
1406 and stores the largest value that fits the declared bit width.

## Deferred behavior

- expression-level numeric and string conversion for `BIT` values
- exact `HEX(bit_column)` and arithmetic-display behavior
- `DEFAULT b'...'` and broader default-expression evaluation
- `ALTER TABLE ... MODIFY` / `CHANGE` conversion of existing `BIT` values
- generated-column, trigger, prepared-statement, and binary-protocol edge cases
- optimizer or physical-index behavior for `BIT` columns

## Tests

Runtime expectations were verified against MySQL 8.4.9 for:

- `CREATE TABLE bit_columns (b BIT, b8 BIT(8), b9 BIT(9), b64 BIT(64))`
- `SHOW CREATE TABLE`
- `SHOW COLUMNS`
- `INFORMATION_SCHEMA.COLUMNS`
- selected-column result metadata
- bit and hex literal inserts into `BIT`, `BIT(8)`, `BIT(9)`, and `BIT(64)`
- fixed-width byte padding for short `BIT(9)` assignments
- strict overflow error 1406
- `INSERT IGNORE` overflow warning 1406 and value clipping
- syntax and descriptor rejection for invalid widths and disallowed attributes

MyLite coverage includes column descriptor tests, parser AST and rejection
tests, and runtime metadata/storage/diagnostic assertions for the same paths.
