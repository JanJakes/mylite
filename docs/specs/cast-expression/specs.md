# CAST expressions

This feature specifies MySQL `CAST(expr AS type)` expression support for
MyLite's shared scalar expression engine. `CAST()` is distinct from ordinary
function calls because the target type is grammar, not a runtime argument.

## Scope

The current implementation supports the application-facing CAST slice:

- `CAST(expr AS SIGNED)` and `CAST(expr AS SIGNED INTEGER)`
- `CAST(expr AS UNSIGNED)` and `CAST(expr AS UNSIGNED INTEGER)`
- `CAST(expr AS DECIMAL)`, `DECIMAL(M)`, `DECIMAL(M,D)`, `DEC(M)`, and
  `DEC(M,D)`
- `CAST(expr AS CHAR)`, `CHAR(N)`, `NCHAR`, and `NCHAR(N)`
- `CAST(expr AS CHAR CHARACTER SET charset_name)` and the `CHARSET` shorthand
  for the initial MyLite charset registry
- `CAST(expr AS BINARY)` and `CAST(expr AS BINARY(N))`, including fixed-length
  byte padding and truncation
- `CAST(expr AS FLOAT)`, `FLOAT(p)`, `FLOAT4`, and `FLOAT4(p)`, where
  precision `p` from `0` through `24` returns `FLOAT` metadata and precision
  `25` through `53` returns `DOUBLE` metadata
- `CAST(expr AS DOUBLE)`, `DOUBLE PRECISION`, default-mode `REAL`, and
  `FLOAT8`; under `REAL_AS_FLOAT`, `REAL` uses the `FLOAT` value and metadata
  path
- `CAST(expr AS DATE)`, `CAST(expr AS TIME)`, `CAST(expr AS TIME(fsp))`,
  `CAST(expr AS DATETIME)`, and `CAST(expr AS DATETIME(fsp))`

The expression must work everywhere the current supported scalar expression
subset works:

- no-table scalar `SELECT`
- one-table `SELECT` projection, `WHERE`, and hidden `ORDER BY` expressions
- supported single-table `UPDATE` assignment, predicate, and order-key
  expressions
- supported single-table `DELETE` predicate and order-key expressions
- inside supported operators, pure scalar functions, and `CASE` expressions

The following behavior is deferred:

- exhaustive binary-string operator semantics for `BINARY expr`; the prefix
  operator is specified in
  `docs/specs/expression-operator-foundation/specs.md`
- `TIMESTAMP` as a direct cast target remains rejected to match MySQL 8.4.9;
  `YEAR`, `JSON`, spatial casts, and `CAST(... AT TIME ZONE ... AS DATETIME)`
  remain deferred
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

Temporal target behavior was additionally checked on 2026-05-07 against the
same MySQL 8.4.9 runtime.

Floating-point target behavior, including `REAL_AS_FLOAT` for `REAL` targets,
was additionally checked on 2026-05-07 against the same MySQL 8.4.9 runtime.
Integer target overflow behavior for approximate inputs was additionally
checked on 2026-05-07 against the same MySQL 8.4.9 runtime.

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
| `CAST(1e20 AS SIGNED)` | `9223372036854775807` | none |
| `CAST(-1e20 AS SIGNED)` | error | 1690 out of range |
| `CAST('12.5' AS SIGNED)` | `12` | 1292 truncated integer |
| `CAST('x' AS SIGNED)` | `0` | 1292 truncated integer |
| `CAST(CONCAT('12', CHAR(0 USING binary), '3') AS SIGNED)` | `12` | 1292 truncated integer |
| `CAST(0x3132 AS SIGNED)` | `12594` | none |
| `CAST(0b1010 AS SIGNED)` | `10` | none |

Unsigned integer casts produce unsigned 64-bit integer results. Negative numeric
inputs wrap to the corresponding two's-complement unsigned value without a
warning. Negative string inputs wrap and also emit warning 1105.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST(-1 AS UNSIGNED)` | `18446744073709551615` | none |
| `CAST(1e20 AS UNSIGNED)` | `9223372036854775807` | none |
| `CAST(-1e20 AS UNSIGNED)` | error | 1690 out of range |
| `CAST('-1' AS UNSIGNED)` | `18446744073709551615` | 1105 negative-to-unsigned |
| `CAST('-1.5' AS UNSIGNED)` | `18446744073709551615` | 1292 truncated integer, 1105 negative-to-unsigned |
| `CAST('x' AS UNSIGNED)` | `0` | 1292 truncated integer |
| `CAST(CONCAT('12', CHAR(0 USING binary), '3') AS UNSIGNED)` | `12` | 1292 truncated integer |
| `CAST(X'3132' AS UNSIGNED)` | `12594` | none |
| `CAST(B'1010' AS UNSIGNED)` | `10` | none |

Decimal casts default to precision 10 and scale 0. `DECIMAL(M)` uses scale 0,
and `DECIMAL(M,D)` uses the supplied scale. Values are rounded to the target
scale. When the rounded result does not fit the requested precision and scale,
the value is clipped to the nearest representable endpoint and warning 1264 is
emitted. Invalid, non-finite, or truncated string inputs emit warning 1292 with
a decimal message. Invalid and non-finite strings produce a zero value formatted
at the target scale; truncated strings keep their parsed numeric prefix before
range validation.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST(12.345 AS DECIMAL)` | `12` | none |
| `CAST(12.345 AS DECIMAL(5))` | `12` | none |
| `CAST(12.345 AS DECIMAL(5,2))` | `12.35` | none |
| `CAST('x' AS DECIMAL(5,2))` | `0.00` | 1292 truncated decimal |
| `CAST(999999 AS DECIMAL(5,2))` | `999.99` | 1264 out of range |
| `CAST(999.995 AS DECIMAL(5,2))` | `999.99` | 1264 out of range |
| `CAST('999999x' AS DECIMAL(5,2))` | `999.99` | 1292 truncated decimal, 1264 out of range |
| `CAST('nan' AS DECIMAL(5,2))` | `0.00` | 1292 truncated decimal |

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
| `HEX(CAST(UNHEX('61FF62') AS CHAR CHARACTER SET utf8mb4))` | `NULL` | 1300 invalid character string |
| `HEX(CAST(UNHEX('61E282AC62') AS CHAR CHARACTER SET utf8mb4))` | `61E282AC62` | none |
| `SET NAMES utf8mb4; HEX(CAST(UNHEX('61FF62') AS CHAR))` | `NULL` | 1300 invalid character string |
| `SET NAMES latin1; HEX(CAST(UNHEX('61FF62') AS CHAR))` | `61FF62` | none |
| `HEX(CAST('a\\0b' AS BINARY))` | `610062` | none |
| `LENGTH(CAST('a\\0b' AS BINARY))` | `3` | none |
| `HEX(CAST('a' AS BINARY(3)))` | `610000` | none |
| `HEX(CAST('abcdef' AS BINARY(3)))` | `616263` | 1292 truncated binary |
| `LENGTH(CAST('x' AS BINARY(0)))` | `0` | 1292 truncated binary |
| `HEX(CAST('a\\0b' AS CHAR))` | `610062` | none |

Floating-point casts parse values through MySQL's DOUBLE conversion path.
`FLOAT` and `FLOAT4` round to single-precision values unless a binary
precision greater than `24` is supplied. `DOUBLE`, `DOUBLE PRECISION`, default
mode `REAL`, and `FLOAT8` return double-precision values. When `REAL_AS_FLOAT`
is active, `REAL` returns `FLOAT` metadata and single-precision rounded values.
Invalid strings return zero and emit warning 1292 with a DOUBLE message. `NULL`
returns `NULL` without warnings.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST('1.23456789' AS FLOAT)` | `1.23457` | none |
| `CAST('1.23456789' AS FLOAT(24))` | `1.23457` | none |
| `CAST('1.23456789' AS FLOAT(25))` | `1.23456789` | none |
| `CAST('1.23456789' AS DOUBLE)` | `1.23456789` | none |
| `CAST('1.23456789' AS REAL)` | `1.23456789` | none |
| `SET sql_mode='REAL_AS_FLOAT'; CAST('1.23456789' AS REAL)` | `1.23457` | none |
| `CAST('1.23456789' AS FLOAT4(10))` | `1.23457` | none |
| `CAST('1.23456789' AS FLOAT8)` | `1.23456789` | none |
| `CAST('x' AS DOUBLE)` | `0` | 1292 truncated double |
| `CAST('12x' AS FLOAT)` | `12` | 1292 truncated double |
| `CAST(NULL AS FLOAT)` | `NULL` | none |

Temporal casts use MySQL's temporal parser and target fractional-second
precision. Fractional values round half up to the requested precision, carrying
into the next second when needed. Missing fractional precision means `0`.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CAST('2024-01-02 03:04:05.123456' AS DATE)` | `2024-01-02` | none |
| `CAST('2024-01-02 03:04:05.987654' AS DATETIME)` | `2024-01-02 03:04:06` | none |
| `CAST('2024-01-02 03:04:05.987654' AS DATETIME(3))` | `2024-01-02 03:04:05.988` | none |
| `CAST('03:04:05.987654' AS TIME(2))` | `03:04:05.99` | none |
| `CAST('838:59:59.400000' AS TIME(6))` | `838:59:59.000000` | 1292 truncated time |
| `CAST('bad' AS DATE)` | `NULL` | 1292 truncated datetime |
| `CAST('bad' AS TIME)` | `NULL` | 1292 truncated time |

Parser errors observed with MySQL 8.4.9:

| SQL | MySQL behavior |
| --- | --- |
| `CAST('1' AS INT)` | syntax error 1064 |
| `CAST('1' AS INTEGER)` | syntax error 1064 |
| `CAST('1' AS NUMERIC(5,2))` | syntax error 1064 |
| `CAST('1' AS DECIMAL(66))` | error 1426 / `42000` |
| `CAST('1' AS DECIMAL(5,6))` | error 1427 / `42000` |
| `CAST('1' AS FLOAT(54))` | error 1426 / `42000` |
| `CAST('1' AS FLOAT(10,2))` | syntax error 1064 |
| `CAST('1' AS DOUBLE(10,2))` | syntax error 1064 |
| `CAST('1' AS FLOAT8(10))` | syntax error 1064 |
| `CAST('x' AS CHAR CHARACTER SET utf8mb4 COLLATE utf8mb4_bin)` | syntax error 1064 |
| `CAST('x' AS TIMESTAMP)` | syntax error 1064 |
| `CAST('x' AS TIME(7))` | error 1426 / `42000` |

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
| `CAST('abc' AS BINARY)` | `VAR_STRING` | source length times connection charset maxlen | `31` | `binary` | `BINARY` |
| `CAST('abc' AS BINARY(3))` | `VAR_STRING` | `3` | `31` | `binary` | `BINARY` |
| `CAST(NULL AS CHAR)` | `VAR_STRING` | `0` | `31` | connection dependent | nullable |
| `CAST('1.25' AS FLOAT)` | `FLOAT` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST('1.25' AS FLOAT(25))` | `DOUBLE` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST('1.25' AS DOUBLE)` | `DOUBLE` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST('1.25' AS REAL)` | `DOUBLE` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CAST(NULL AS FLOAT)` | `FLOAT` | `23` | `31` | `binary` | nullable `BINARY NUM` |
| `CAST('2024-01-02' AS DATE)` | `DATE` | `10` | `0` | `binary` | `BINARY` |
| `CAST('2024-01-02 03:04:05' AS DATETIME(3))` | `DATETIME` | `23` | `3` | `binary` | `BINARY` |
| `CAST('03:04:05' AS TIME(2))` | `TIME` | `13` | `2` | `binary` | `BINARY` |
| `CAST(NULL AS DATETIME(6))` | `DATETIME` | `26` | `6` | `binary` | nullable `BINARY` |

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
cast_target_type ::= FLOATKW opt_cast_float_precision.
cast_target_type ::= FLOAT4 opt_cast_float_precision.
cast_target_type ::= DOUBLE opt_precision_keyword.
cast_target_type ::= REAL.
cast_target_type ::= FLOAT8.
cast_target_type ::= CHAR opt_column_length opt_cast_character_set.
cast_target_type ::= NCHAR opt_column_length.
cast_target_type ::= BINARY opt_column_length.
cast_target_type ::= DATE.
cast_target_type ::= TIME opt_temporal_fsp.
cast_target_type ::= DATETIME opt_temporal_fsp.

opt_integer_keyword ::= .
opt_integer_keyword ::= INTEGERKW.

opt_cast_float_precision ::= .
opt_cast_float_precision ::= column_precision.

opt_cast_character_set ::= .
opt_cast_character_set ::= CHARACTER SET charset_value.
opt_cast_character_set ::= CHARSET charset_value.
```

`COLLATE` inside the cast target remains a syntax error. Applying standalone
expression-level `COLLATE` to the cast result is supported for registered
collations.

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
- hex and bit literal inputs use the literal's unsigned integer value, not the
  decoded binary string bytes
- string inputs parse an optional sign and decimal integer prefix
- string suffixes, decimal fractions, and missing digits emit warning 1292
  using `Truncated incorrect INTEGER value: '<value>'`
- embedded NUL bytes are part of the input; bytes after the parsed numeric
  prefix still trigger warning 1292

### Unsigned integer

- integer and real inputs convert to the unsigned 64-bit representation
- real inputs round halves away from zero before conversion
- hex and bit literal inputs use the literal's unsigned integer value, not the
  decoded binary string bytes
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
- values that do not fit the requested precision after rounding are clipped to
  the signed target endpoint and emit warning 1264 using the cast expression as
  the diagnostic column text
- invalid or truncated strings emit warning 1292 using
  `Truncated incorrect DECIMAL value: '<value>'`
- invalid and non-finite strings return zero formatted at the target scale;
  truncated strings keep their parsed numeric prefix before range validation

### Floating point

- integer, unsigned integer, real, and text inputs convert through MyLite's
  DOUBLE-compatible numeric conversion path
- invalid or truncated string inputs emit warning 1292 using
  `Truncated incorrect DOUBLE value: '<value>'`
- `FLOAT` and `FLOAT4` without precision, and with precision `0` through `24`,
  round to the nearest single-precision value before display
- `FLOAT(p)` and `FLOAT4(p)` with precision `25` through `53` return
  double-precision values and metadata
- `DOUBLE`, `DOUBLE PRECISION`, default-mode `REAL`, and `FLOAT8` return
  double-precision values and metadata

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

### Temporal

- `DATE` uses the shared date/datetime parser and formats the date component
- `TIME(fsp)` uses the shared time parser, rounds fractional seconds to `fsp`,
  and formats the result with exactly the requested fractional digits
- out-of-range `TIME` values clip to MySQL's `838:59:59` endpoint with warning
  1292
- `DATETIME(fsp)` uses the shared date/datetime parser, rounds fractional
  seconds to `fsp`, and formats the result with exactly the requested
  fractional digits
- omitted `fsp` means `0`; accepted precision is `0` through `6`
- invalid temporal input returns `NULL` and emits warning 1292

## Metadata

CAST expression metadata is derived from the target type, not from the runtime
value except where MySQL reports source-length-derived character or binary
length and the source is statically known.

MyLite descriptors:

- `SIGNED`: `LONGLONG`, length `21`, decimals `0`, binary charset, `BINARY NUM`
- `UNSIGNED`: same as signed plus `UNSIGNED`
- `DECIMAL`: `NEWDECIMAL`, decimals from target, binary charset, `BINARY NUM`,
  length `precision + 1` when scale is zero and `precision + 2` otherwise
- `FLOAT` / `FLOAT4` with precision up to `24`: `FLOAT`, length `23`,
  decimals `31`, binary charset, `BINARY NUM`
- `FLOAT(p)` / `FLOAT4(p)` with precision `25` through `53`, `DOUBLE`,
  `DOUBLE PRECISION`, default-mode `REAL`, and `FLOAT8`: `DOUBLE`, length
  `23`, decimals `31`, binary charset, `BINARY NUM`
- `CHAR`: `VAR_STRING`, decimals `31`, connection charset, nullable expression
  metadata
- `CHAR(N)`: same as `CHAR`, length `N` times the connection charset max byte
  width
- `CHAR CHARACTER SET binary` and `BINARY`: `VAR_STRING`, decimals `31`,
  binary charset, `BINARY`; no-length literal binary casts keep MySQL's
  connection-width display length while length-qualified binary casts use the
  requested binary length
- `DATE`: `DATE`, length `10`, decimals `0`, binary charset, `BINARY`
- `TIME(fsp)`: `TIME`, length `10` without fractions or `11 + fsp` with
  fractions, decimals from target, binary charset, `BINARY`
- `DATETIME(fsp)`: `DATETIME`, length `19` without fractions or `20 + fsp`
  with fractions, decimals from target, binary charset, `BINARY`

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
- `CAST('2024-01-02' AS DATE)`, `CAST('03:04:05.987654' AS TIME(2))`,
  and `CAST('2024-01-02 03:04:05.987654' AS DATETIME(6))`
- `CAST('1.25' AS FLOAT)`, `FLOAT(24)`, `FLOAT(25)`, `FLOAT4`,
  `FLOAT4(25)`, `DOUBLE`, `DOUBLE PRECISION`, `REAL`, and `FLOAT8`
- expression-level `COLLATE` after a supported character cast
- nested casts and casts inside `CASE`
- syntax errors for `INT`, `INTEGER`, `NUMERIC`, `COLLATE` inside the target,
  missing `AS`, missing target type, invalid decimal precision/scale, invalid
  temporal precision, invalid floating display-scale syntax, `FLOAT8(p)`, and
  direct `TIMESTAMP` targets

Runtime tests:

- no-table `SELECT` results for every supported target
- `NULL` input propagation
- signed numeric rounding and string truncation warnings
- unsigned wrapping and negative-string warning
- decimal default precision/scale, explicit scale rounding, invalid string
  warning, and result text formatting
- character truncation and `CHAR(0)` warnings
- floating `FLOAT`/`DOUBLE`/`REAL`/`FLOAT4`/`FLOAT8` values, truncation
  warnings, `NULL` propagation, and metadata
- temporal date/datetime/time parsing, fractional-second rounding, invalid
  input warnings, and `CONVERT(expr, temporal_type)` delegation
- metadata descriptors for signed, unsigned, decimal, char, binary, nullable
  char, date, time, and datetime casts
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment/predicate/order and strict warning promotion
- `DELETE` predicate/order and strict warning promotion
- binding of invalid identifiers inside cast sources even when the cast appears
  in an unselected `CASE` branch

## Compatibility notes

This task is intentionally a high-value CAST subset rather than a complete type
conversion engine. The main known differences are:

- decimal runtime values are formatted text with decimal metadata, not native
  fixed-point values
- fixed-length binary padding/truncation and complete binary-string operator
  semantics remain deferred beyond length-aware preservation of source bytes
  for the supported `CAST(... AS BINARY)` form
- connection charset metadata is limited to the current MyLite charset registry
- `TIMESTAMP` direct targets remain syntax errors as in MySQL 8.4.9; `YEAR`,
  JSON, spatial, and timezone-aware casts are separate tasks
- `REAL_AS_FLOAT` is applied to expression-level `REAL` cast targets, but
  broader approximate-numeric DDL and storage-mode behavior remains deferred
- overflow and SQL-mode diagnostics are not exhaustive

`CONVERT(expr, type)` and `CONVERT(expr USING charset_name)` are implemented as
the CAST-family extension specified in
`docs/specs/convert-function/specs.md`.
