# CAST expressions

This feature specifies MySQL `CAST(expr AS type)` expression support for
MyLite's shared scalar expression engine. `CAST()` is distinct from ordinary
function calls because the target type is grammar, not a runtime argument.

## Scope

Task 26 implements the first application-facing CAST slice:

- `CAST(expr AS SIGNED)` and `CAST(expr AS SIGNED INTEGER)`
- `CAST(expr AS UNSIGNED)` and `CAST(expr AS UNSIGNED INTEGER)`
- `CAST(expr AS DECIMAL)`, `DECIMAL(M)`, `DECIMAL(M,D)`, `DEC(M)`, and
  `DEC(M,D)`
- `CAST(expr AS CHAR)`, `CHAR(N)`, `NCHAR`, and `NCHAR(N)`
- `CAST(expr AS CHAR CHARACTER SET charset_name)` and the `CHARSET` shorthand
  for the initial MyLite charset registry
- `CAST(expr AS BINARY)` as a binary-string metadata cast without fixed-length
  zero padding

The expression must work everywhere the current supported scalar expression
subset works:

- no-table scalar `SELECT`
- one-table `SELECT` projection, `WHERE`, and hidden `ORDER BY` expressions
- supported single-table `UPDATE` assignment, predicate, and order-key
  expressions
- supported single-table `DELETE` predicate and order-key expressions
- inside supported operators, pure scalar functions, and `CASE` expressions

The following behavior is deferred:

- `BINARY expr` prefix operator
- fixed-length binary padding/truncation fidelity for `CAST(... AS BINARY(N))`
  because MyLite's public value API does not yet expose binary lengths
- `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `YEAR`, `FLOAT`, `DOUBLE`, `REAL`,
  `JSON`, spatial casts, and `CAST(... AT TIME ZONE ... AS DATETIME)`
- multi-valued-index `ARRAY` casts
- exhaustive overflow/range clipping and every SQL-mode variant

## References

This specification is independently authored from official MySQL 8.4 material
and MySQL 8.4.9 runtime observations:

- MySQL 8.4 Reference Manual, Cast Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Connection Character Sets and Collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset-connection.html
- MySQL 8.4 Reference Manual, Fixed-Point Types:
  https://dev.mysql.com/doc/refman/8.4/en/fixed-point-types.html

Runtime expectations were checked on 2026-05-01 against `mylite-mysql-849`
using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --force --batch --raw --show-warnings`

## MySQL observations

`CAST(NULL AS <supported type>)` returns `NULL` with the target type's metadata.

Signed integer casts produce signed 64-bit integer results. Numeric floating
inputs round to the nearest integer with halves away from zero. String inputs
parse an optional sign and integer prefix; a decimal point or nonnumeric suffix
produces warning 1292 and the integer prefix is used. A string with no integer
prefix returns `0` with warning 1292.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST(12.4 AS SIGNED)` | `12` | none |
| `CAST(12.5 AS SIGNED)` | `13` | none |
| `CAST(-12.5 AS SIGNED)` | `-13` | none |
| `CAST('12.5' AS SIGNED)` | `12` | 1292 truncated integer |
| `CAST('x' AS SIGNED)` | `0` | 1292 truncated integer |

Unsigned integer casts produce unsigned 64-bit integer results. Negative numeric
inputs wrap to the corresponding two's-complement unsigned value without a
warning. Negative string inputs wrap and also emit warning 1105.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST(-1 AS UNSIGNED)` | `18446744073709551615` | none |
| `CAST('-1' AS UNSIGNED)` | `18446744073709551615` | 1105 negative-to-unsigned |
| `CAST('-1.5' AS UNSIGNED)` | `18446744073709551615` | 1292 truncated integer, 1105 negative-to-unsigned |
| `CAST('x' AS UNSIGNED)` | `0` | 1292 truncated integer |

Decimal casts default to precision 10 and scale 0. `DECIMAL(M)` uses scale 0,
and `DECIMAL(M,D)` uses the supplied scale. Values are rounded to the target
scale. Invalid or truncated string inputs emit warning 1292 with a decimal
message and produce a zero value formatted at the target scale.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST(12.345 AS DECIMAL)` | `12` | none |
| `CAST(12.345 AS DECIMAL(5))` | `12` | none |
| `CAST(12.345 AS DECIMAL(5,2))` | `12.35` | none |
| `CAST('x' AS DECIMAL(5,2))` | `0.00` | 1292 truncated decimal |

Character casts produce variable-length character-string metadata. `CHAR(N)`
limits the result to at most `N` characters and emits warning 1292 when
truncation occurs. It does not pad shorter values. `CHAR(0)` returns an empty
string and warns when the source is nonempty. `CHARACTER SET binary` produces
binary-string metadata.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST(38.8 AS CHAR)` | `38.8` | none |
| `CAST('abcdef' AS CHAR(3))` | `abc` | 1292 truncated char |
| `CAST('abc' AS CHAR(5))` | `abc` | none |
| `CAST('abc' AS CHAR CHARACTER SET binary)` | `abc` | none |

Parser errors observed with MySQL 8.4.9:

| SQL | MySQL behavior |
| --- | --- |
| `CAST('1' AS INT)` | syntax error 1064 |
| `CAST('1' AS INTEGER)` | syntax error 1064 |
| `CAST('1' AS NUMERIC(5,2))` | syntax error 1064 |
| `CAST('1' AS DECIMAL(66))` | error 1426 / `42000` |
| `CAST('1' AS DECIMAL(5,6))` | error 1427 / `42000` |
| `CAST('x' AS CHAR CHARACTER SET utf8mb4 COLLATE utf8mb4_bin)` | syntax error 1064 |

Metadata observations from `mysql --column-type-info -vvv`:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | --- | --- | --- | --- |
| `CAST('123' AS SIGNED)` | `LONGLONG` | `21` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST('123' AS UNSIGNED)` | `LONGLONG` | `21` | `0` | `binary` | `NOT_NULL UNSIGNED BINARY NUM` |
| `CAST('12' AS DECIMAL)` | `NEWDECIMAL` | `11` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST('12' AS DECIMAL(5))` | `NEWDECIMAL` | `6` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST('12.34' AS DECIMAL(6,2))` | `NEWDECIMAL` | `8` | `2` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST('abc' AS CHAR)` | `VAR_STRING` | source length times charset maxlen | `31` | connection dependent | none |
| `CAST('abc' AS CHAR(3))` | `VAR_STRING` | `3` times charset maxlen | `31` | connection dependent | none |
| `CAST('abc' AS BINARY)` | `VAR_STRING` | source length | `31` | `binary` | `BINARY` |
| `CAST(NULL AS CHAR)` | `VAR_STRING` | `0` | `31` | connection dependent | nullable |

## Syntax

MyLite should parse CAST in the shared expression grammar as a primary
expression. The first implementation intentionally accepts only the supported
target types listed above.

MyLite Lemon-style grammar snippet:

```lemon
primary_expression ::= cast_expression.

cast_expression ::= CAST LPAREN expression AS cast_target_type RPAREN.

cast_target_type ::= SIGNED opt_integer_keyword.
cast_target_type ::= UNSIGNED opt_integer_keyword.
cast_target_type ::= DECIMALKW opt_numeric_precision_scale.
cast_target_type ::= DEC opt_numeric_precision_scale.
cast_target_type ::= CHAR opt_column_length opt_cast_character_set.
cast_target_type ::= NCHAR opt_column_length.
cast_target_type ::= BINARY.

opt_integer_keyword ::= .
opt_integer_keyword ::= INTEGERKW.

opt_cast_character_set ::= .
opt_cast_character_set ::= CHARACTER SET charset_value.
opt_cast_character_set ::= CHARSET charset_value.
```

`COLLATE` inside the cast target remains a syntax error. Applying a future
standalone `COLLATE` operator to the result is separate work.

## AST

Add `MYLITE_SQL_AST_CAST_EXPRESSION`.

The cast node has two children:

1. source expression
2. target `MYLITE_SQL_AST_COLUMN_TYPE` node

The target node reuses the existing column-type fields:

- `column_type`
- `has_column_length` / `column_length`
- `has_column_precision` / `column_precision`
- `has_column_scale` / `column_scale`
- `column_type_signed` / `column_type_unsigned`
- `has_column_character_set` / `column_character_set`
- `column_national_attribute`

## Runtime semantics

The evaluator first evaluates the source expression. If the source is `NULL`,
the result is `NULL` without conversion warnings.

### Signed integer

- integer inputs keep their numeric value, with unsigned overflow clamped to
  the current MyLite numeric conversion behavior until full range diagnostics
  land
- real inputs round halves away from zero
- string inputs parse an optional sign and decimal integer prefix
- string suffixes, decimal fractions, and missing digits emit warning 1292
  using `Truncated incorrect INTEGER value: '<value>'`

### Unsigned integer

- integer and real inputs convert to the unsigned 64-bit representation
- real inputs round halves away from zero before conversion
- string inputs follow signed string parsing first
- negative string inputs emit warning 1105 with
  `Cast to unsigned converted negative integer to its positive complement`
- string truncation emits warning 1292 before the unsigned conversion warning

### Decimal

The first implementation stores decimal cast runtime values as text formatted
at the target scale, while the metadata descriptor exposes `NEWDECIMAL`. This
preserves result text and keeps existing arithmetic/comparison paths usable
through their string-to-number conversion. Full fixed-point arithmetic is
deferred to the broader decimal type task.

- missing precision means `(10,0)`
- missing scale means `0`
- values round to the target scale
- invalid or truncated strings emit warning 1292 using
  `Truncated incorrect DECIMAL value: '<value>'`
- invalid strings return zero formatted at the target scale

### Character

- numeric sources are converted to character text
- `CHAR(N)` truncates by character count, not by byte count, for valid UTF-8
  input handled by the current expression engine
- truncation emits warning 1292 using
  `Truncated incorrect CHAR(N) value: '<value>'`
- `NCHAR` sets the target as a national character cast in the AST but maps to
  the current connection charset descriptor until the national charset task
  expands the registry
- `CHAR CHARACTER SET binary` returns the same bytes as text with binary
  metadata; fixed-length binary padding remains deferred

## Metadata

CAST expression metadata is derived from the target type, not from the runtime
value except where MySQL reports source-length-derived character or binary
length and the source is statically known.

MyLite descriptors:

- `SIGNED`: `LONGLONG`, length `21`, decimals `0`, binary charset, `BINARY NUM`
- `UNSIGNED`: same as signed plus `UNSIGNED`
- `DECIMAL`: `NEWDECIMAL`, decimals from target, binary charset, `BINARY NUM`,
  length `precision + 1` when scale is zero and `precision + 2` otherwise
- `CHAR`: `VAR_STRING`, decimals `31`, connection charset, nullable if source
  can be null
- `CHAR(N)`: same as `CHAR`, length `N * maxlen_for_charset`
- `CHAR CHARACTER SET binary` and `BINARY`: `VAR_STRING`, decimals `31`,
  binary charset, `BINARY`

Origin schema/table/column metadata is empty for CAST results.

## Binding and validation

Bind walkers must visit the source expression even when the cast result is not
eventually evaluated because hidden branches still validate in MySQL. The target
type is parser-owned metadata and does not bind identifiers.

Unsupported target syntax should fail at parse time for now. Unsupported future
target types may later move to execution diagnostics if parsing the full MySQL
grammar becomes more important than early rejection.

## Tests

Parser tests:

- `CAST(1 AS SIGNED)`, `SIGNED INTEGER`, `UNSIGNED`, and `UNSIGNED INTEGER`
- `CAST('1' AS DECIMAL)`, `DECIMAL(5)`, `DECIMAL(5,2)`, `DEC(5,2)`
- `CAST(38.8 AS CHAR)`, `CHAR(3)`, `CHAR CHARACTER SET utf8mb4`,
  `CHAR CHARACTER SET binary`, `NCHAR(4)`, and `BINARY`
- nested casts and casts inside `CASE`
- syntax errors for `INT`, `INTEGER`, `NUMERIC`, `COLLATE` inside the target,
  missing `AS`, missing target type, and invalid decimal precision/scale

Runtime tests:

- no-table `SELECT` results for every supported target
- `NULL` input propagation
- signed numeric rounding and string truncation warnings
- unsigned wrapping and negative-string warning
- decimal default precision/scale, explicit scale rounding, invalid string
  warning, and result text formatting
- character truncation and `CHAR(0)` warnings
- metadata descriptors for signed, unsigned, decimal, char, binary, and nullable
  char casts
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment/predicate/order and strict warning promotion
- `DELETE` predicate/order and strict warning promotion
- binding of invalid identifiers inside cast sources even when the cast appears
  in an unselected `CASE` branch

## Compatibility notes

This task is intentionally a high-value CAST subset rather than a complete type
conversion engine. The main known differences after Task 26 are:

- decimal runtime values are formatted text with decimal metadata, not native
  fixed-point values
- binary strings cannot yet preserve embedded NUL bytes through the public
  text-only value API
- connection charset metadata is limited to the current MyLite charset registry
- temporal, JSON, spatial, and timezone-aware casts are separate tasks
- overflow and SQL-mode diagnostics are not exhaustive

`CONVERT(expr, type)` and `CONVERT(expr USING charset_name)` are implemented as
the CAST-family extension specified in
`docs/specs/convert-function/specs.md`.
