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
- `CONVERT(expr, BINARY)` as the current MyLite binary-string metadata slice
- `CONVERT(expr USING charset_name)` for the current MyLite charset registry:
  `binary`, `latin1`, `utf8mb3` / `utf8`, and `utf8mb4`

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

- temporal, JSON, spatial, and timezone-aware casts
- multi-valued-index `ARRAY` casts
- fixed-length binary padding/truncation fidelity
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

## MySQL observations

The ODBC-style `CONVERT(expr, type)` form is equivalent to
`CAST(expr AS type)` for supported cast targets. It returns `NULL` when `expr`
is `NULL` and otherwise follows the CAST result, warning, and metadata rules.

| SQL | Result | Warnings |
| --- | --- | --- |
| `CONVERT('12.5', SIGNED)` | `12` | 1292 truncated integer |
| `CONVERT('-1', UNSIGNED)` | `18446744073709551615` | 1105 negative-to-unsigned |
| `CONVERT('12.345', DECIMAL(5,2))` | `12.35` | none |
| `CONVERT('abcdef', CHAR(3))` | `abc` | 1292 truncated char |
| `CONVERT('abc', BINARY)` | `abc` | none |
| `CONVERT(NULL, SIGNED)` | `NULL` | none |

The `CONVERT(expr USING charset_name)` form returns `NULL` when `expr` is
`NULL`. Otherwise, MySQL returns string bytes exposed with the requested
charset and that charset's default collation. In MyLite's current slice, the
runtime value preserves the source text bytes and updates charset/collation
introspection metadata for supported charsets.

| SQL | Result |
| --- | --- |
| `CHARSET(CONVERT('abc' USING latin1))` | `latin1` |
| `COLLATION(CONVERT('abc' USING latin1))` | `latin1_swedish_ci` |
| `COERCIBILITY(CONVERT('abc' USING latin1))` | `2` |
| `CHARSET(CONVERT('abc' USING binary))` | `binary` |
| `COLLATION(CONVERT('abc' USING binary))` | `binary` |
| `CONVERT(NULL USING utf8mb4)` | `NULL` |
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
| `CONVERT('abc', CHAR)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | none |
| `CONVERT('abc', CHAR(3))` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | none |
| `CONVERT('abc', BINARY)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `binary` | `BINARY` |
| `CONVERT('abc' USING latin1)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | none |
| `CONVERT('abc' USING binary)` under `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `binary` | `BINARY` |

MySQL column metadata for non-binary explicit charset conversions can remain
connection-collation based even though `CHARSET()`, `COLLATION()`, and
`COERCIBILITY()` expose the requested charset/collation. MyLite currently
derives CAST column descriptors from the cast target for explicit charsets.
This task preserves that existing CAST architecture and documents the
non-binary column-metadata mismatch as a deferred CAST-family compatibility gap.

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
target with `has_column_character_set`. `USING binary` therefore follows the
same binary-string path as `CAST(expr AS CHAR CHARACTER SET binary)`, which is
compatible with MyLite's current `CAST(expr AS BINARY)` subset for the covered
runtime value and introspection behavior.

The syntax span remains the original `CONVERT(...)` expression so diagnostics
and debug output identify the source SQL accurately.

## Runtime semantics

The evaluator first evaluates the source expression. If the source is `NULL`,
the result is `NULL` without conversion warnings.

`CONVERT(expr, type)` delegates to the existing CAST evaluator for the target:

- signed and unsigned integer behavior matches CAST
- decimal rounding and string truncation warnings match CAST
- char length truncation warnings match CAST
- binary conversion matches the current MyLite `CAST(... AS BINARY)` subset

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

For `CONVERT(expr USING binary)`, the descriptor matches the current
`CAST(expr AS BINARY)` subset: `VAR_STRING`, binary charset/collation,
`decimals = 31`, and `BINARY` flag.

For `CONVERT(expr USING nonbinary_charset)`, MySQL column metadata is
connection-collation based in the verified literal cases, while introspection
functions report the requested charset and default collation. MyLite will
initially reuse the CAST target descriptor path, so explicit non-binary charset
column descriptors can reflect the target charset rather than the connection
charset. Runtime introspection is the compatibility surface covered in this
task; exact non-binary column metadata is deferred with the broader CAST
metadata gaps.

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
- `CONVERT('abc' USING latin1)`, `utf8mb4`, quoted `utf8mb3`, `utf8`, and
  `binary`
- expression-level `COLLATE` after `CONVERT(... USING ...)` and
  `CONVERT(..., CHAR CHARACTER SET ...)`
- nested `CONVERT` and `CONVERT` inside `CASE`
- syntax errors for `INT`, empty argument list, one argument, three arguments,
  missing comma, missing `USING` charset, `COLLATE` inside the target, and
  invalid decimal precision/scale; unknown or charset-incompatible
  expression-level collations fail during preparation

Runtime tests:

- no-table `SELECT` results for every supported ODBC target and `USING`
  charset
- `NULL` input propagation for both forms
- signed string truncation warnings
- unsigned negative-string complement warnings
- decimal scale rounding
- char truncation and `CHAR(0)` warnings
- invalid charset diagnostics for both forms
- metadata descriptors for signed, unsigned, decimal, char, binary, and
  nullable `USING`
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
- non-binary explicit charset result column metadata can follow existing MyLite
  CAST target descriptors instead of MySQL's verified connection-collation
  descriptor behavior
- binary strings cannot yet preserve embedded NUL bytes through every public
  text-value path
- fixed-length binary padding remains deferred
- temporal, JSON, spatial, and timezone-aware casts are separate tasks
- overflow and SQL-mode diagnostics remain incomplete in the same places as
  the existing CAST implementation
