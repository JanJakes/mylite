# CONVERT cast expressions

This feature specifies MySQL `CONVERT()` cast-expression support for MyLite's
shared scalar expression engine. MySQL exposes two unrelated `CONVERT` forms:
an ODBC-style cast form and a character-set conversion form. Both are grammar
constructs rather than ordinary scalar function calls.

## Scope

This task implements:

- `CONVERT(expr, SIGNED)` and `CONVERT(expr, SIGNED INTEGER)`
- `CONVERT(expr, UNSIGNED)` and `CONVERT(expr, UNSIGNED INTEGER)`
- `CONVERT(expr, DECIMAL)`, `DECIMAL(M)`, `DECIMAL(M,D)`, `DEC(M)`, and
  `DEC(M,D)`
- `CONVERT(expr, CHAR)`, `CHAR(N)`, `CHARACTER SET charset_name`, and
  `CHARSET charset_name`
- `CONVERT(expr, NCHAR)` and `NCHAR(N)`
- `CONVERT(expr, BINARY)` and `CONVERT(expr, BINARY(N))`
- `CONVERT(expr, FLOAT)`, `FLOAT(p)`, `FLOAT4`, `FLOAT4(p)`, `DOUBLE`,
  `DOUBLE PRECISION`, default-mode `REAL`, and `FLOAT8`; under
  `REAL_AS_FLOAT`, `REAL` uses the `FLOAT` value and metadata path
- `CONVERT(expr, DATE)`, `CONVERT(expr, TIME)`, `CONVERT(expr, TIME(fsp))`,
  `CONVERT(expr, DATETIME)`, and `CONVERT(expr, DATETIME(fsp))`
- `CONVERT(expr USING charset_name)` for the current MyLite expression-level
  charset set: `binary`, `latin1`, `utf8mb3` / `utf8`, `utf8mb4`, and `ascii`

The expression must work everywhere the supported scalar expression subset
already evaluates `CAST`:

- no-table scalar `SELECT`
- one-table `SELECT` projection, `WHERE`, and hidden `ORDER BY` expressions
- supported single-table `UPDATE` assignment, predicate, and order-key
  expressions
- supported single-table `DELETE` predicate and order-key expressions
- inside supported operators, pure scalar functions, and `CASE` expressions

The following behavior is deferred with the same compatibility boundaries as
`docs/specs/cast-expression/specs.md`:

- `TIMESTAMP` as a direct cast target remains rejected to match MySQL 8.4.9;
  `YEAR`, JSON, spatial, and timezone-aware casts remain deferred
- multi-valued-index `ARRAY` casts
- full byte transcoding between character sets
- exhaustive overflow/range diagnostics and every SQL-mode variant
- `COLLATE` inside `CONVERT` target syntax, which MySQL rejects; applying
  expression-level `COLLATE` to the result is supported for registered
  collations

## References

This specification is independently authored from official MySQL 8.4 material
and MySQL 8.4.9 runtime observations:

- MySQL 8.4 Reference Manual, Cast Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html
- MySQL 8.4 Reference Manual, Column Character Set Conversion:
  https://dev.mysql.com/doc/refman/8.4/en/charset-conversion.html
- Existing MyLite CAST expression spec:
  `docs/specs/cast-expression/specs.md`
- Existing MyLite charset/collation foundation spec:
  `docs/specs/character-set-collation-foundation/specs.md`
- Existing MyLite charset/collation introspection functions spec:
  `docs/specs/charset-collation-functions/specs.md`

Runtime expectations were checked on 2026-05-04 against `mylite-mysql-849`
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

The ODBC-style `CONVERT(expr, type)` form is equivalent to
`CAST(expr AS type)` for supported cast targets. It returns `NULL` when `expr`
is `NULL` and otherwise follows the CAST result, warning, and metadata rules.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CONVERT('12.5', SIGNED)` | `12` | 1292 truncated integer |
| `CONVERT(-1e20, SIGNED)` | error | 1690 out of range |
| `CONVERT('-1', UNSIGNED)` | `18446744073709551615` | 1105 negative-to-unsigned |
| `CONVERT(-1e20, UNSIGNED)` | error | 1690 out of range |
| `CONVERT('12.345', DECIMAL(5,2))` | `12.35` | none |
| `CONVERT(999999, DECIMAL(5,2))` | `999.99` | 1264 out of range |
| `CONVERT('999999x', DECIMAL(5,2))` | `999.99` | 1292 truncated decimal, 1264 out of range |
| `CONVERT('abcdef', CHAR(3))` | `abc` | 1292 truncated char |
| `CONVERT('abc', BINARY)` | `abc` | none |
| `HEX(CONVERT('a', BINARY(3)))` | `610000` | none |
| `HEX(CONVERT('abcdef', BINARY(3)))` | `616263` | 1292 truncated binary |
| `HEX(CONVERT(UNHEX('E282AC62'), CHAR(3) CHARACTER SET binary))` | `E282AC` | 1292 truncated binary |
| `CONVERT('1.23456789', FLOAT)` | `1.23457` | none |
| `CONVERT('1.23456789', FLOAT(25))` | `1.23456789` | none |
| `CONVERT('1.23456789', DOUBLE)` | `1.23456789` | none |
| `CONVERT('1.23456789', REAL)` | `1.23456789` | none |
| `SET sql_mode='REAL_AS_FLOAT'; CONVERT('1.23456789', REAL)` | `1.23457` | none |
| `CONVERT('x', DOUBLE)` | `0` | 1292 truncated double |
| `CONVERT(NULL, SIGNED)` | `NULL` | none |
| `CONVERT('2024-01-02 03:04:05.123456', DATE)` | `2024-01-02` | none |
| `CONVERT('2024-01-02 03:04:05.123456', DATETIME(6))` | `2024-01-02 03:04:05.123456` | none |
| `CONVERT('03:04:05.987654', TIME)` | `03:04:06` | none |

The `CONVERT(expr USING charset_name)` form returns `NULL` when `expr` is
`NULL`. Otherwise, MySQL returns string bytes exposed with the requested
charset and that charset's default collation. In MyLite's current slice, the
runtime value preserves the source text bytes and updates charset/collation
introspection metadata for supported charsets. Explicit `utf8mb4` and
`utf8mb3` targets validate the resulting byte sequence; invalid input returns
`NULL` with warning 1300. `CONVERT(expr, CHAR)` uses the connection character
set for the same validation decision.

| SQL | Result |
| --- | --- |
| `CHARSET(CONVERT('abc' USING latin1))` | `latin1` |
| `COLLATION(CONVERT('abc' USING latin1))` | `latin1_swedish_ci` |
| `COERCIBILITY(CONVERT('abc' USING latin1))` | `2` |
| `CHARSET(CONVERT('abc' USING binary))` | `binary` |
| `COLLATION(CONVERT('abc' USING binary))` | `binary` |
| `CONVERT(NULL USING utf8mb4)` | `NULL` |
| `HEX(CONVERT(UNHEX('61FF62') USING utf8mb4))` | `NULL`, warning 1300 |
| `HEX(CONVERT(UNHEX('61E282AC62') USING utf8mb4))` | `61E282AC62` |
| `HEX(CONVERT(UNHEX('61FF62') USING binary))` | `61FF62` |
| `HEX(CONVERT(UNHEX('61FF62') USING ascii))` | `61FF62` |
| `COLLATION(CONVERT('abc' USING ascii))` | `ascii_general_ci` |
| `SET NAMES utf8mb4; HEX(CONVERT(UNHEX('61FF62'), CHAR))` | `NULL`, warning 1300 |
| `HEX(CONVERT(UNHEX('61FF62'), CHAR CHARACTER SET ascii))` | `61FF62` |
| `SET NAMES latin1; HEX(CONVERT(UNHEX('61FF62'), CHAR))` | `61FF62` |
| `SET NAMES latin1; HEX(CONVERT(UNHEX('E282AC62'), CHAR(1)))` | `E2`, warning 1292 |
| `COLLATION(CONVERT('abc' USING latin1) COLLATE latin1_bin)` | `latin1_bin` |
| `COERCIBILITY(CONVERT('abc' USING latin1) COLLATE latin1_bin)` | `0` |
| `COLLATION(CONVERT('abc' USING utf8mb4) COLLATE utf8mb4_bin)` | `utf8mb4_bin` |

`utf8` is accepted as MySQL 8.4's compatibility alias for `utf8mb3`. MySQL
returns the converted value with `utf8mb3` charset/collation introspection and
emits warning `3719`:

| SQL | Result | Warning |
| --- | --- | --- |
| `CHARSET(CONVERT('abc' USING utf8))` | `utf8mb3` | `3719` |
| `COLLATION(CONVERT('abc' USING utf8))` | `utf8mb3_general_ci` | `3719` |
| `CONVERT('abc' USING utf8)` | `abc` | `3719` |

Parser and validation errors observed with MySQL 8.4.9:

| SQL | MySQL behavior |
| --- | --- |
| `CONVERT('abc' USING nosuchcharset)` | error 1115 / `42000` |
| `CONVERT('abc', CHAR CHARACTER SET nosuchcharset)` | error 1115 / `42000` |
| `CONVERT('1', INT)` | syntax error 1064 / `42000` |
| `CONVERT('1', FLOAT(54))` | error 1426 / `42000` |
| `CONVERT('1', FLOAT(10,2))` | syntax error 1064 / `42000` |
| `CONVERT('1', DOUBLE(10,2))` | syntax error 1064 / `42000` |
| `CONVERT('1', FLOAT8(10))` | syntax error 1064 / `42000` |
| `CONVERT()` | syntax error 1064 / `42000` |
| `CONVERT(1)` | syntax error 1064 / `42000` |
| `CONVERT(1, SIGNED, 2)` | syntax error 1064 / `42000` |
| `CONVERT('abc' USING latin1) COLLATE utf8mb4_bin` | error 1253 / `42000` |

Metadata observations from `mysql --column-type-info -vvv`:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | --- | --- | --- | --- |
| `CONVERT('123', SIGNED)` | `LONGLONG` | `21` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `CONVERT('123', UNSIGNED)` | `LONGLONG` | `21` | `0` | `binary` | `NOT_NULL UNSIGNED BINARY NUM` |
| `CONVERT('12.34', DECIMAL(6,2))` | `NEWDECIMAL` | `8` | `2` | `binary` | `NOT_NULL BINARY NUM` |
| `CONVERT('1.25', FLOAT)` | `FLOAT` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CONVERT('1.25', FLOAT(25))` | `DOUBLE` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CONVERT('1.25', DOUBLE)` | `DOUBLE` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CONVERT('1.25', REAL)` under `REAL_AS_FLOAT` | `FLOAT` | `23` | `31` | `binary` | `NOT_NULL BINARY NUM` |
| `CONVERT('abc', CHAR)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | none |
| `CONVERT('abc', CHAR(3))` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | none |
| `CONVERT('abc', BINARY)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `binary` | `BINARY` |
| `CONVERT('abc', BINARY(3))` under `SET NAMES utf8mb4` | `VAR_STRING` | `3` | `31` | `binary` | `BINARY` |
| `CONVERT('abc' USING latin1)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | none |
| `CONVERT('abc' USING binary)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `binary` | `BINARY` |
| `CONVERT('2024-01-02', DATE)` | `DATE` | `10` | `0` | `binary` | `BINARY` |
| `CONVERT('2024-01-02 03:04:05', DATETIME(6))` | `DATETIME` | `26` | `6` | `binary` | `BINARY` |
| `CONVERT('03:04:05', TIME(2))` | `TIME` | `13` | `2` | `binary` | `BINARY` |

MySQL column metadata for explicit charset conversions remains
connection-collation based even though `CHARSET()`, `COLLATION()`, and
`COERCIBILITY()` expose the requested charset/collation. Binary conversions use
binary result metadata but, for no-length literal casts, keep MySQL's
connection-width display length.

## Syntax

MyLite should parse both forms as primary expressions. Unsupported target
families remain syntax errors for this implementation.

MyLite Lemon-style grammar snippet:

```lemon
primary_expression ::= convert_expression.

convert_expression ::= CONVERT LPAREN expression COMMA cast_target_type RPAREN.
convert_expression ::= CONVERT LPAREN expression USING charset_value RPAREN.
collate_expression ::= collate_expression COLLATE charset_value.
```

The first production reuses the `cast_target_type` grammar from the CAST
expression spec. The `USING` form constructs an internal character cast target
with the requested charset. `USING binary` therefore uses the same
`CHARACTER SET binary` path as the current CAST-family binary-string subset.

## AST

Reuse `MYLITE_SQL_AST_CAST_EXPRESSION`.

The node has two children:

1. source expression
2. target `MYLITE_SQL_AST_COLUMN_TYPE` node

`CONVERT(expr, type)` stores the parsed `cast_target_type` as the target.
`CONVERT(expr USING charset_name)` stores a `MYLITE_SQL_AST_COLUMN_TYPE_CHAR`
target with `has_column_character_set`. `USING binary` and length-qualified
`CHAR(N) CHARACTER SET binary` targets therefore follow the same binary-string
path as `CAST(expr AS CHAR CHARACTER SET binary)`, including fixed-length byte
truncation and padding for the covered runtime value and introspection
behavior.

The syntax span remains the original `CONVERT(...)` expression so diagnostics
and debug output identify the source SQL accurately.

## Runtime semantics

The evaluator first evaluates the source expression. If the source is `NULL`,
the result is `NULL` without conversion warnings.

`CONVERT(expr, type)` delegates to the existing CAST evaluator for the target:

- signed and unsigned integer behavior matches CAST
- decimal rounding and string truncation warnings match CAST
- char length truncation warnings match CAST
- binary conversion matches `CAST(... AS BINARY)` and
  `CAST(... AS BINARY(N))`
- floating-point conversion, single-precision rounding, and metadata match
  CAST
- date, time, and datetime parsing, fractional-second rounding, warnings, and
  metadata match CAST

`CONVERT(expr USING charset_name)`:

- validates `charset_name` against MyLite's charset registry
- normalizes `utf8` to `utf8mb3` and records MySQL warning `3719`
- returns `NULL` for `NULL` input
- converts non-`NULL` input to MyLite text bytes using the same string
  conversion path as character casts
- does not perform cross-charset byte transcoding in this slice
- exposes the requested charset and default collation to
  `CHARSET()`, `COLLATION()`, and `COERCIBILITY()`
- treats `binary` as the existing binary-string cast target

Unknown charsets return the current MyLite unknown-character-set diagnostic,
aligned with MySQL error 1115 / `42000` once numeric code and SQLSTATE exposure
is complete.

## Metadata

For `CONVERT(expr, type)`, expression descriptors match the existing CAST
descriptor rules for the same target.

For `CONVERT(expr USING binary)`, the descriptor matches
`CAST(expr AS BINARY)`: `VAR_STRING`, binary charset/collation,
`decimals = 31`, and `BINARY` flag.

For `CONVERT(expr USING nonbinary_charset)`, MySQL column metadata is
connection-collation based in the verified literal cases, while introspection
functions report the requested charset and default collation. MyLite follows
that split: result-column descriptors expose the connection collation and
connection-width display length, while runtime introspection reports the
requested charset/collation.

Origin schema/table/column metadata is empty for all CONVERT results.

## Binding and validation

Bind walkers must visit the source expression even when the converted result is
not selected at runtime, because MySQL still validates identifiers inside
unselected expression branches. The target type and charset name are parser
metadata and do not bind identifiers.

`CONVERT(expr USING charset_name)` validates the charset during prepare-time
metadata or collation inference whenever the expression participates in result
metadata or charset/collation introspection. Runtime evaluation also validates
before producing a non-`NULL` value so DML predicates and assignments cannot
silently accept unknown charsets.

## Tests

Parser tests:

- `CONVERT('123', SIGNED)`, `SIGNED INTEGER`, `UNSIGNED`, and
  `UNSIGNED INTEGER`
- `CONVERT('1.25', DECIMAL)`, `DECIMAL(5)`, `DECIMAL(5,2)`, and `DEC(5,2)`
- `CONVERT('abcdef', CHAR(3))`, `CHAR CHARACTER SET latin1`,
  `CHAR CHARSET utf8mb4`, `NCHAR(4)`, and `BINARY`
- `CONVERT('2024-01-02', DATE)`, `CONVERT('03:04:05.987654', TIME(2))`,
  and `CONVERT('2024-01-02 03:04:05.987654', DATETIME(6))`
- `CONVERT('1.25', FLOAT)`, `FLOAT(25)`, `FLOAT4`, `DOUBLE`,
  `DOUBLE PRECISION`, `REAL`, and `FLOAT8`
- `CONVERT('abc' USING latin1)`, `utf8mb4`, quoted `utf8mb3`, `utf8`, and
  `binary`
- invalid `utf8mb4` byte validation for `CONVERT(... USING utf8mb4)` and
  `CONVERT(..., CHAR CHARACTER SET utf8mb4)`, plus connection-character-set
  validation for `CONVERT(..., CHAR)`
- expression-level `COLLATE` after `CONVERT(... USING ...)` and
  `CONVERT(..., CHAR CHARACTER SET ...)`
- nested `CONVERT` and `CONVERT` inside `CASE`
- syntax errors for `INT`, empty argument list, one argument, three arguments,
  missing comma, missing `USING` charset, `COLLATE` inside the target, and
  invalid decimal precision/scale, invalid floating display-scale syntax, and
  `FLOAT8(p)`; unknown or charset-incompatible expression-level collations fail
  during preparation

Runtime tests:

- no-table `SELECT` results for every supported ODBC target and `USING`
  charset
- `NULL` input propagation for both forms
- signed string truncation warnings
- unsigned negative-string complement warnings
- decimal scale rounding
- floating `FLOAT`/`DOUBLE`/`REAL`/`FLOAT4`/`FLOAT8` values, truncation
  warnings, `NULL` propagation, and metadata
- char truncation and `CHAR(0)` warnings
- temporal date/datetime/time parsing, fractional-second rounding, invalid
  input warnings, and metadata through the shared CAST evaluator
- invalid charset diagnostics for both forms
- metadata descriptors for signed, unsigned, decimal, floating-point, char,
  binary, and nullable `USING`
- charset/collation/coercibility introspection for `USING latin1`,
  `USING utf8`, `USING utf8mb4`, `USING binary`, and
  `CHAR CHARACTER SET latin1`
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment, predicate, and order-key expressions, including strict
  warning promotion
- `DELETE` predicate and order-key expressions, including strict warning
  promotion
- binding of invalid identifiers inside converted sources even when the
  expression appears in an unselected `CASE` branch

## Compatibility notes

This task is intentionally a CAST-family extension, not a full charset
transcoding project. Known differences after this implementation:

- `CONVERT(... USING nonbinary_charset)` preserves text bytes rather than
  transcoding between character sets
- binary strings cannot yet preserve embedded NUL bytes through every public
  text-value path
- `TIMESTAMP` direct targets remain syntax errors as in MySQL 8.4.9; `YEAR`,
  JSON, spatial, and timezone-aware casts are separate tasks
- `REAL_AS_FLOAT` is applied to expression-level `REAL` cast targets, but
  broader approximate-numeric DDL and storage-mode behavior remains deferred
- overflow and SQL-mode diagnostics remain incomplete in the same places as
  the existing CAST implementation
