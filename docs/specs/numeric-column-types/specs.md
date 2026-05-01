# Exact and approximate numeric column types

## Scope

This feature extends MyLite's parse-only `CREATE TABLE` column type foundation
with MySQL exact and approximate numeric declarations:

- `DECIMAL`, `DEC`, `NUMERIC`, and `FIXED`
- `FLOAT`, `DOUBLE`, `DOUBLE PRECISION`, `REAL`, `FLOAT4`, and `FLOAT8`
- optional precision and scale syntax accepted by MySQL 8.4.9
- `SIGNED`, `UNSIGNED`, and `ZEROFILL` numeric type attributes
- internal descriptor metadata for `DATA_TYPE`, `COLUMN_TYPE`,
  `NUMERIC_PRECISION`, and `NUMERIC_SCALE`
- parse-only `CREATE TABLE` column definitions for the covered declarations

Table DDL execution, catalog writes, value storage, arithmetic, rounding,
warnings, and information-schema row creation remain later roadmap work. Valid
declarations prepare as `MYLITE_UNSUPPORTED`; malformed declarations fail
during parsing.

## Sources

- MySQL 8.4 Reference Manual, Numeric Data Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, Fixed-Point Types:
  https://dev.mysql.com/doc/refman/8.4/en/fixed-point-types.html
- MySQL 8.4 Reference Manual, Floating-Point Types:
  https://dev.mysql.com/doc/refman/8.4/en/floating-point-types.html
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

Exact numeric declarations normalize to lowercase `decimal` metadata:

| Declaration | `DATA_TYPE` | `COLUMN_TYPE` | Precision | Scale |
| --- | --- | --- | ---: | ---: |
| `DECIMAL` | `decimal` | `decimal(10,0)` | 10 | 0 |
| `DECIMAL(10)` | `decimal` | `decimal(10,0)` | 10 | 0 |
| `DECIMAL(10,2)` | `decimal` | `decimal(10,2)` | 10 | 2 |
| `DEC`, `NUMERIC`, `FIXED` | `decimal` | canonical decimal form | declaration dependent | declaration dependent |

`DECIMAL(0)` and `DECIMAL(0,0)` are accepted and normalize to
`decimal(10,0)`. Precision `1..65` is accepted. Scale `0..30` is accepted when
it is not greater than precision. `DECIMAL(66)` errors with MySQL error 1426,
`DECIMAL(65,31)` errors with 1425, and `DECIMAL(10,11)` errors with 1427.
Negative precision or missing precision/scale positions are syntax errors.
Oversized integer tokens must be rejected without wrapping.

Approximate numeric declarations normalize as follows in the default SQL mode:

| Declaration | `DATA_TYPE` | `COLUMN_TYPE` | Precision | Scale |
| --- | --- | --- | ---: | --- |
| `FLOAT` | `float` | `float` | 12 | `NULL` |
| `FLOAT(10)` | `float` | `float` | 12 | `NULL` |
| `FLOAT(25)` | `double` | `double` | 22 | `NULL` |
| `FLOAT(10,2)` | `float` | `float(10,2)` | 10 | 2 |
| `DOUBLE`, `DOUBLE PRECISION` | `double` | `double` | 22 | `NULL` |
| `DOUBLE(10,2)` | `double` | `double(10,2)` | 10 | 2 |
| `REAL` | `double` | `double` | 22 | `NULL` |
| `FLOAT4` | `float` | `float` | 12 | `NULL` |
| `FLOAT8` | `double` | `double` | 22 | `NULL` |

`FLOAT(p)` is a binary-precision selector: `0..24` maps to `float`, `25..53`
maps to `double`, and `54` errors with 1063. `FLOAT(M,D)`, `DOUBLE(M,D)`,
`REAL(M,D)`, `FLOAT4(M,D)`, and `FLOAT8(M,D)` use display/scale metadata:
`M` may be `1..255`, `D` may be `0..30`, and `M` must be at least `D`.
`FLOAT(255,30)` and `DOUBLE(255,30)` are accepted with deprecation warning
1681. `FLOAT(256,30)` and `DOUBLE(256,30)` error with 1439.
`DOUBLE(10)` is a syntax error.

`SIGNED` has no metadata effect. `UNSIGNED` appends ` unsigned` to
`COLUMN_TYPE`. `ZEROFILL` appends ` unsigned zerofill` and implies unsigned
even when `SIGNED` appears before or after it. `FLOAT(25) UNSIGNED` maps to
`double unsigned`, while `FLOAT(25,2) UNSIGNED` remains `float(25,2) unsigned`.
MySQL emits deprecation warnings for `UNSIGNED` on decimal/floating types,
`ZEROFILL`, and floating `(M,D)`; MyLite warning storage is deferred.

`REAL` is affected by `REAL_AS_FLOAT`: with `SET SESSION sql_mode='REAL_AS_FLOAT'`,
`REAL` reports `float` metadata. MyLite has no session-aware DDL execution in
this parse-only task, so default-mode `REAL` metadata is implemented now and
`REAL_AS_FLOAT` handling is deferred.

## MyLite behavior

### Type descriptor

MyLite extends the internal descriptor with a numeric domain. Given a type
keyword, optional precision, optional scale, and numeric attributes, it returns:

- canonical numeric family
- lowercase `DATA_TYPE`
- MySQL-compatible `COLUMN_TYPE`
- effective signedness and zerofill state
- whether the declaration used an alias such as `DEC`, `NUMERIC`, `FIXED`,
  `DOUBLE PRECISION`, `REAL`, `FLOAT4`, or `FLOAT8`
- numeric precision
- numeric scale when MySQL reports one, or an explicit null-scale flag for
  default approximate declarations

Descriptor normalization is internal and not part of the public ABI.

### Parser and AST

MyLite accepts the existing narrow `CREATE TABLE` shape:

```sql
CREATE TABLE table_name (
    column_name column_type
    [, column_name column_type ...]
)
```

Column attributes such as `NULL`, `NOT NULL`, `DEFAULT`, comments, generated
columns, inline keys, table constraints, table options, temporary tables,
`CREATE TABLE ... LIKE`, and `CREATE TABLE ... SELECT` remain out of scope.

Accepted type syntax for this task:

```sql
exact_numeric_type ::= exact_numeric_name [ ( precision [ , scale ] ) ] numeric_attribute_list
exact_numeric_name ::= DECIMAL | DEC | NUMERIC | FIXED

float_type ::= FLOAT [ ( precision [ , scale ] ) ] numeric_attribute_list
float_type ::= FLOAT4 [ ( precision [ , scale ] ) ] numeric_attribute_list

double_type ::= DOUBLE [ PRECISION ] [ ( precision , scale ) ] numeric_attribute_list
double_type ::= REAL [ ( precision , scale ) ] numeric_attribute_list
double_type ::= FLOAT8 [ ( precision , scale ) ] numeric_attribute_list

numeric_attribute_list ::= [ numeric_attribute ... ]
numeric_attribute ::= SIGNED
numeric_attribute ::= UNSIGNED
numeric_attribute ::= ZEROFILL
```

`precision` and `scale` use unsigned integer tokens only. Negative values fail
as syntax errors. MyLite performs parser-time range checks for the declaration
limits covered by this parse-only feature.

### Runtime boundary

Preparing a parse-only `CREATE TABLE` statement covered by this feature returns
`MYLITE_UNSUPPORTED`, not `MYLITE_PARSE_ERROR`. No SQLite table is created and
no MyLite catalog rows are written. Task 11 owns execution, metadata writes,
implicit commit semantics, warnings, and statement side effects.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
column_type ::= exact_numeric_column_type.
column_type ::= float_column_type.
column_type ::= double_column_type.

exact_numeric_column_type ::= exact_numeric_type_name opt_numeric_precision_scale
                              numeric_type_attribute_list.
exact_numeric_type_name ::= DECIMAL.
exact_numeric_type_name ::= DEC.
exact_numeric_type_name ::= NUMERIC.
exact_numeric_type_name ::= FIXED.

float_column_type ::= FLOAT opt_float_precision_or_scale numeric_type_attribute_list.
float_column_type ::= FLOAT4 opt_float_precision_or_scale numeric_type_attribute_list.

double_column_type ::= DOUBLE opt_precision_keyword opt_double_scale
                       numeric_type_attribute_list.
double_column_type ::= REAL opt_double_scale numeric_type_attribute_list.
double_column_type ::= FLOAT8 opt_double_scale numeric_type_attribute_list.

opt_numeric_precision_scale ::= .
opt_numeric_precision_scale ::= LPAREN INTEGER RPAREN.
opt_numeric_precision_scale ::= LPAREN INTEGER COMMA INTEGER RPAREN.

opt_float_precision_or_scale ::= .
opt_float_precision_or_scale ::= LPAREN INTEGER RPAREN.
opt_float_precision_or_scale ::= LPAREN INTEGER COMMA INTEGER RPAREN.

opt_double_scale ::= .
opt_double_scale ::= LPAREN INTEGER COMMA INTEGER RPAREN.

opt_precision_keyword ::= .
opt_precision_keyword ::= PRECISION.

numeric_type_attribute_list ::= .
numeric_type_attribute_list ::= numeric_type_attribute_list numeric_type_attribute.
numeric_type_attribute ::= SIGNED.
numeric_type_attribute ::= UNSIGNED.
numeric_type_attribute ::= ZEROFILL.
```

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or declaration | Expected MySQL-compatible outcome |
| --- | --- |
| `DECIMAL`, `DECIMAL(10)`, `DECIMAL(10,2)` | decimal metadata with default or declared precision/scale |
| `DEC`, `NUMERIC(8,3)`, `FIXED(7,2)` | normalize to `decimal` metadata |
| `DECIMAL(0)`, `DECIMAL(0,0)` | normalize to `decimal(10,0)` |
| `DECIMAL(65,30)`, `DECIMAL(30,30)` | accepted |
| `DECIMAL(66)`, `DECIMAL(65,31)`, `DECIMAL(10,11)` | parse error for this parse-only feature |
| `DECIMAL(-1)`, `DECIMAL(10,)`, `DECIMAL(,2)` | syntax error |
| `DECIMAL(18446744073709551616)` | parse error without integer wraparound |
| `FLOAT`, `FLOAT(10)`, `FLOAT(24)` | `float`, precision 12, null scale |
| `FLOAT(25)`, `FLOAT(53)` | `double`, precision 22, null scale |
| `FLOAT(54)` | parse error |
| `FLOAT(10,2)`, `FLOAT(255,30)` | `float(M,D)` metadata |
| `FLOAT(0,0)`, `DOUBLE(0,0)` | parse error |
| `FLOAT(0,1)`, `DOUBLE(0,1)`, `DECIMAL(0,1)` | parse error |
| `DOUBLE`, `DOUBLE PRECISION`, `DOUBLE(10,2)` | `double` metadata |
| `DOUBLE(10)`, `DOUBLE(10) ZEROFILL` | syntax error |
| `REAL`, `FLOAT4`, `FLOAT8` | default-mode aliases to `double`, `float`, and `double` |
| `UNSIGNED`, mixed `SIGNED`/`UNSIGNED`, and `ZEROFILL` | unsigned metadata when any `UNSIGNED` or `ZEROFILL` appears |
| `FLOAT(25) UNSIGNED` | `double unsigned` |
| `FLOAT(25,2) UNSIGNED` | `float(25,2) unsigned` |

## Compatibility gaps

- Table DDL execution, catalog writes, storage, rounding, overflow, arithmetic,
  and value conversion are deferred.
- Warning records for deprecated floating `(M,D)`, decimal/floating
  `UNSIGNED`, and `ZEROFILL` are deferred until diagnostics/warning storage
  exists.
- `REAL_AS_FLOAT` is deferred until column DDL is session-aware.
- Protocol-level column flags and prepared-statement metadata are deferred to
  the result metadata task.
