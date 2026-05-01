# Integer and boolean column types

## Scope

This feature adds MyLite's first column type foundation for MySQL integer and
boolean column declarations:

- `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`, `INTEGER`, and `BIGINT`
- integer aliases `INT1`, `INT2`, `INT3`, `INT4`, `INT8`, and `MIDDLEINT`
- optional integer display width syntax
- `SIGNED` and `UNSIGNED` integer attributes
- `BOOL` and `BOOLEAN` aliases
- internal type-descriptor metadata for ranges and
  `INFORMATION_SCHEMA.COLUMNS` fields
- parse-only `CREATE TABLE` column definitions for these types

Full `CREATE TABLE` execution, column attributes, defaults, constraints,
`ZEROFILL`, `AUTO_INCREMENT`, storage writes, and information-schema row
creation are later roadmap tasks. This task may accept the syntax and return
`MYLITE_UNSUPPORTED` during prepare or execution until Task 11 implements table
DDL.

## Sources

- MySQL 8.4 Reference Manual, Numeric Data Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, Integer Types:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html
- MySQL 8.4 Reference Manual, Numeric Type Attributes:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-attributes.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

MySQL accepts five integer storage classes and exposes them in lowercase
canonical metadata:

| Authored family | Canonical `DATA_TYPE` | Signed precision | Unsigned precision |
| --- | --- | ---: | ---: |
| `TINYINT`, `INT1` | `tinyint` | 3 | 3 |
| `SMALLINT`, `INT2` | `smallint` | 5 | 5 |
| `MEDIUMINT`, `MIDDLEINT`, `INT3` | `mediumint` | 7 | 7 |
| `INT`, `INTEGER`, `INT4` | `int` | 10 | 10 |
| `BIGINT`, `INT8` | `bigint` | 19 | 20 |

All integer families have `NUMERIC_SCALE=0`. `COLUMN_TYPE` is the canonical
lowercase type name with ` unsigned` appended when the effective type is
unsigned.

`INTEGER` normalizes to `int`. `MIDDLEINT` normalizes to `mediumint`.
`INT1`, `INT2`, `INT3`, `INT4`, and `INT8` normalize to `tinyint`,
`smallint`, `mediumint`, `int`, and `bigint` respectively. `INT0`, `INT5`,
`INT6`, `INT7`, and `INT9` are syntax errors.

Integers are signed by default. `SIGNED` is accepted and has no metadata effect.
`UNSIGNED` shifts the value range upward and appends ` unsigned` in
`COLUMN_TYPE`. If both `SIGNED` and `UNSIGNED` are present in either order, the
verified runtime reports the column as unsigned.

Integer display width syntax accepts an unsigned decimal width from `0` through
`255`. `INT(256)` fails with display-width-out-of-range error 1439. `INT(-1)`
is a syntax error. MySQL 8.4.9 accepts display width for compatibility but
normally omits it from `COLUMN_TYPE` and `SHOW CREATE TABLE`. The verified
exception is signed `TINYINT(1)`, which reports `COLUMN_TYPE='tinyint(1)'` and
prints `tinyint(1)`. `TINYINT(1) UNSIGNED` reports `tinyint unsigned`.

`BOOL` and `BOOLEAN` are aliases for signed `TINYINT(1)`. They report
`DATA_TYPE='tinyint'`, `COLUMN_TYPE='tinyint(1)'`, `NUMERIC_PRECISION=3`, and
`NUMERIC_SCALE=0`. In the verified runtime, `BOOL SIGNED`, `BOOL UNSIGNED`,
`BOOL(1)`, `BOOLEAN SIGNED`, `BOOLEAN UNSIGNED`, and `BOOLEAN(1)` are syntax
errors. Truth-value expression behavior is not part of this column-type task.

The value ranges to preserve in MyLite descriptors are:

| Type | Signed range | Unsigned range |
| --- | --- | --- |
| `tinyint` | `-128` to `127` | `0` to `255` |
| `smallint` | `-32768` to `32767` | `0` to `65535` |
| `mediumint` | `-8388608` to `8388607` | `0` to `16777215` |
| `int` | `-2147483648` to `2147483647` | `0` to `4294967295` |
| `bigint` | `-9223372036854775808` to `9223372036854775807` | `0` to `18446744073709551615` |

The unsigned `bigint` maximum does not fit in signed 64-bit storage. MyLite
should keep range endpoints as text or another split representation until the
value system has a full unsigned integer representation.

## MyLite behavior

### Type descriptor

MyLite adds a lean internal descriptor for integer and boolean column types.
Given a type keyword, optional display width, and integer signedness
attributes, it returns:

- canonical type family
- lowercase `DATA_TYPE`
- MySQL-compatible `COLUMN_TYPE`
- effective signedness
- whether the declaration came from `BOOL` or `BOOLEAN`
- display width when syntactically provided or implied by boolean aliases
- storage byte count
- numeric precision and scale
- signed and unsigned range endpoints as text
- the effective range endpoints for the signedness

Descriptor normalization is internal and not part of the public ABI.

The descriptor rejects:

- unknown integer aliases
- display widths greater than `255`
- display width or signedness attributes on `BOOL` and `BOOLEAN`

The parser rejects malformed syntax where MySQL treats the form as syntactic,
including negative display widths, `BOOL(1)`, `BOOL UNSIGNED`, and unknown
reserved/nonreserved aliases in type position.

### Parser and AST

MyLite accepts the following narrow `CREATE TABLE` shape for parser and AST
coverage:

```sql
CREATE TABLE table_name (
    column_name integer_or_boolean_type
    [, column_name integer_or_boolean_type ...]
)
```

`table_name` may be qualified in the same way as existing table references.
Column names may be unquoted or backtick quoted identifiers. Only the integer
and boolean type syntax from this feature is accepted. Column attributes such
as `NULL`, `NOT NULL`, `DEFAULT`, `AUTO_INCREMENT`, comments, generated
columns, inline keys, table constraints, table options, temporary tables,
`CREATE TABLE ... LIKE`, and `CREATE TABLE ... SELECT` remain out of scope.

Accepted integer type syntax:

```sql
integer_type_name [ ( display_width ) ] [ integer_signedness ... ]

integer_type_name ::= TINYINT | SMALLINT | MEDIUMINT | INT | INTEGER | BIGINT
integer_type_name ::= INT1 | INT2 | INT3 | INT4 | INT8 | MIDDLEINT
integer_signedness ::= SIGNED | UNSIGNED
```

Accepted boolean type syntax:

```sql
BOOL
BOOLEAN
```

### Runtime boundary

Preparing a parse-only `CREATE TABLE` statement covered by this feature returns
`MYLITE_UNSUPPORTED`, not `MYLITE_PARSE_ERROR`. No SQLite table is created and
no MyLite catalog rows are written. Task 11 owns execution, metadata writes,
implicit commit semantics, warnings, and statement side effects.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
statement ::= create_table_statement.

create_table_statement ::= CREATE TABLE table_name LPAREN column_definition_list RPAREN.

column_definition_list ::= column_definition.
column_definition_list ::= column_definition_list COMMA column_definition.

column_definition ::= identifier integer_or_boolean_column_type.

integer_or_boolean_column_type ::= integer_column_type.
integer_or_boolean_column_type ::= boolean_column_type.

integer_column_type ::= integer_type_name opt_integer_display_width.
integer_column_type ::= integer_column_type integer_signedness.

integer_type_name ::= TINYINT.
integer_type_name ::= SMALLINT.
integer_type_name ::= MEDIUMINT.
integer_type_name ::= INT.
integer_type_name ::= INTEGER.
integer_type_name ::= BIGINT.
integer_type_name ::= INT1.
integer_type_name ::= INT2.
integer_type_name ::= INT3.
integer_type_name ::= INT4.
integer_type_name ::= INT8.
integer_type_name ::= MIDDLEINT.

opt_integer_display_width ::= .
opt_integer_display_width ::= LPAREN INTEGER RPAREN.

integer_signedness ::= SIGNED.
integer_signedness ::= UNSIGNED.

boolean_column_type ::= BOOL.
boolean_column_type ::= BOOLEAN.
```

## MySQL-runtime-verified expectations

The implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or declaration | Expected MySQL-compatible outcome |
| --- | --- |
| `TINYINT` | `DATA_TYPE=tinyint`, `COLUMN_TYPE=tinyint`, precision `3`, scale `0` |
| `TINYINT(1)` | `COLUMN_TYPE=tinyint(1)` |
| `TINYINT(1) UNSIGNED` | `COLUMN_TYPE=tinyint unsigned` |
| `SMALLINT` | precision `5` |
| `MEDIUMINT` | precision `7` |
| `INT`, `INTEGER` | `DATA_TYPE=int`, precision `10` |
| `BIGINT` | signed precision `19` |
| `BIGINT UNSIGNED` | unsigned precision `20` |
| `BOOL`, `BOOLEAN` | `DATA_TYPE=tinyint`, `COLUMN_TYPE=tinyint(1)` |
| `INT1`, `INT2`, `INT3`, `INT4`, `INT8`, `MIDDLEINT` | normalize to `tinyint`, `smallint`, `mediumint`, `int`, `bigint`, `mediumint` |
| `INT SIGNED UNSIGNED`, `INT UNSIGNED SIGNED` | `COLUMN_TYPE=int unsigned` |
| `INT(0)`, `INT(255)` | accepted, `COLUMN_TYPE=int` |
| `INT(256)` | display width out of range |
| `INT(-1)` | syntax error |
| `BOOL(1)`, `BOOL SIGNED`, `BOOLEAN UNSIGNED` | syntax error |
| `INT0`, `INT5`, `INT6`, `INT7`, `INT9` | syntax error |

## Compatibility gaps

- Table DDL execution is intentionally not implemented in this feature.
- `ZEROFILL` is deferred to a later numeric attributes task.
- Column attributes, constraints, defaults, indexes, and metadata catalog
  writes are deferred.
- Out-of-range insert/update behavior waits for DML and value conversion work.
- Protocol-level column flags and prepared-statement metadata are deferred to
  the result metadata task.
