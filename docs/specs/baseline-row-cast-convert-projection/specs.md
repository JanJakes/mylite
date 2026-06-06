# Baseline Row CAST/CONVERT Projection

## Purpose

This slice admits the already-parsed scalar `CAST()` and `CONVERT()` forms in
single-table row-scalar `SELECT` projections. The goal is to close the common
WordPress-driver shape:

```sql
SELECT CONVERT(column_name, BINARY), CONVERT(column_name USING utf8mb4),
       CONVERT(integer_column, SIGNED)
FROM table_name
```

without opening a general expression engine, predicate casts, DML assignment
casts, or wider charset support.

## Compatibility Authorities

- Official MySQL 8.4 reference manual, cast functions:
  <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
- Official MySQL 8.4 reference manual, character-set introducers and `COLLATE`:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-collate.html>
- Official MySQL 8.4 reference manual, collation coercibility:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-collation-coercibility.html>
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

The MyLite grammar and runtime behavior below are independently authored from
the documentation and runtime observations. No MySQL implementation or grammar
source is used.

## Supported Surface

Admit these expressions in descriptor-driven single-table row-scalar `SELECT`
lists, including projections with `WHERE`, `ORDER BY`, and `LIMIT` already
supported by the row-scalar path:

```sql
CAST(value AS BINARY)
CAST(value AS CHAR)
CAST(value AS DATE)
CAST(value AS SIGNED)
CAST(value AS SIGNED INTEGER)
CAST(value AS SIGNED INT)
CAST(value AS UNSIGNED)
CAST(value AS UNSIGNED INTEGER)
CAST(value AS UNSIGNED INT)
CONVERT(value, BINARY)
CONVERT(value, CHAR)
CONVERT(value, DATE)
CONVERT(value, SIGNED)
CONVERT(value, SIGNED INTEGER)
CONVERT(value, SIGNED INT)
CONVERT(value, UNSIGNED)
CONVERT(value, UNSIGNED INTEGER)
CONVERT(value, UNSIGNED INT)
CONVERT(value USING BINARY)
CONVERT(value USING utf8mb4)
CONVERT(value USING utf8)
CONVERT(value USING utf8mb3)
CONVERT(value USING latin1)
CONVERT(value USING 'utf8mb4')
CONVERT(value USING 'utf8')
CONVERT(value USING 'utf8mb3')
CONVERT(value USING 'latin1')
```

The admitted `value` forms are:

- unqualified or table-qualified descriptor columns from the single selected
  source table;
- row-scalar string, signed 64-bit integer, boolean, and `NULL` literals;
- supported statement scalar values that the existing row-scalar planner can
  fold to a parameter.

Descriptor column operands are limited to MyLite integer-family columns stored
in the current physical range and nonbinary string-family columns. Binary
string, `BIT`, approximate numeric, `DECIMAL`, temporal, `YEAR`, JSON, generated
column, subquery, arithmetic, and function operands are deferred unless they
already fold to one of the scalar literal/parameter shapes above.

Nested conversion chains are limited to nonnumeric target steps in this slice.
Nested `SIGNED` or `UNSIGNED` conversion chains are rejected until MyLite has a
typed intermediate expression representation that can preserve MySQL's numeric
conversion boundary rather than feeding text output back into the next step.

## Parser And AST

The parser already owns the grammar for this slice through the scalar
`CAST()`/`CONVERT()` productions:

```lemon
expression(A) ::= CAST(T) LPAREN expression(V) AS BINARY RPAREN(R).
expression(A) ::= CAST(T) LPAREN expression(V) AS cast_basic_target(K) RPAREN(R).
expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA BINARY RPAREN(R).
expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA cast_basic_target(K) RPAREN(R).
expression(A) ::= CONVERT(T) LPAREN expression(V) USING BINARY RPAREN(R).
expression(A) ::= CONVERT(T) LPAREN expression(V) USING option_name(C) RPAREN(R).
```

This phase does not add `CHAR(N)`, `BINARY(N)`, explicit cast charset clauses,
bare `INT`/`INTEGER` target names, full temporal target semantics,
decimal/approximate targets, JSON/spatial targets, or postfix row `COLLATE`.
The `DATE` target is accepted as a compatibility placeholder and uses the
current character-conversion path.

## Runtime Semantics

The analyzer resolves column operands from MyLite descriptors, not SQLite
metadata. Unknown columns use the existing MySQL-compatible unknown-column
diagnostic. Unsupported operand descriptor kinds use deterministic capability
diagnostics.

`CAST(... AS BINARY)`, `CONVERT(..., BINARY)`, and `CONVERT(... USING BINARY)`
return the operand's MySQL text representation as binary string bytes.

`CAST(... AS CHAR)`, `CONVERT(..., CHAR)`, `CAST(... AS DATE)`,
`CONVERT(..., DATE)`, and
`CONVERT(... USING utf8mb4|utf8|utf8mb3|latin1)` return the operand's MySQL
text representation as a nonbinary string with the target metadata. For this
slice, `latin1` conversion remains ASCII-only like the scalar charset baseline;
non-ASCII row values fail with a MyLite-specific capability diagnostic until a
real transcoding slice exists.

`CAST(... AS SIGNED)`, `CONVERT(..., SIGNED)`, `CAST(... AS UNSIGNED)`, and
`CONVERT(..., UNSIGNED)` perform MyLite-owned integer conversion:

- `NULL` returns `NULL`;
- integer-family descriptor values convert from their numeric value and do not
  emit truncation or complement warnings for negative signed-to-unsigned or
  unsigned-width-to-signed conversions observed as numeric values;
- string values skip leading ASCII whitespace, accept one optional sign,
  consume a decimal digit prefix, ignore trailing ASCII whitespace, and warn
  when no digits, malformed sign text, nonspace trailing text, or range clipping
  occurs;
- positive string magnitudes above signed `BIGINT` max and at or below unsigned
  `BIGINT` max cast to signed as the signed complement and warn with MySQL's
  complement warning;
- negative string magnitudes cast to unsigned as the unsigned complement and
  warn with MySQL's complement warning.

Successful row conversions are lowered to generated SQLite SQL calling MyLite
SQLite scalar functions. SQLite supplies row values and filtering/order/limit
iteration; MyLite owns conversion, warnings, target metadata, identifier
quoting, and diagnostics. The implementation must not materialize the input
table in MyLite memory, mutate catalog rows, or require SQLite fork changes.
Unsigned conversion results that fit SQLite's signed 64-bit integer range are
returned to SQLite as integer values so generated `ORDER BY
CAST(... AS UNSIGNED)` expressions sort ordinary values numerically. Values
above signed `BIGINT` range continue to use unsigned decimal text so projection
readback can represent the full MySQL unsigned 64-bit domain.

## Metadata

Result-column metadata follows the conversion target:

- binary conversions: `VAR_STRING`, binary charset/collation id `63`, binary
  flag, nullable;
- character conversions: `VAR_STRING` with the connection collation for
  `CHAR`, `utf8mb4_0900_ai_ci` for `utf8mb4`, `utf8mb3_general_ci` for `utf8`
  and `utf8mb3`, and `latin1_swedish_ci` for `latin1`;
- the `DATE` placeholder uses character-conversion values but exposes
  MySQL-shaped `DATE` result metadata for source-backed row-scalar projections;
- signed conversions: signed `LONGLONG`, binary charset/collation id `63`,
  numeric and binary flags, nullable;
- unsigned conversions: unsigned `LONGLONG`, binary charset/collation id `63`,
  numeric, binary, and unsigned flags, nullable.

String-collation planning treats character conversions as string-collated and
binary/numeric conversions as bytewise or numeric.

## Warnings And Diagnostics

Runtime-verified warnings:

| Case | Warning |
| --- | --- |
| `CONVERT(... USING utf8)` | `3719 / HY000`, once per expression |
| `CONVERT(... USING utf8mb3)` | `1287 / HY000`, once per expression |
| truncated string-to-integer row value | `1292 / 22007`, per converted row |
| string positive unsigned-width value cast to signed | `1105 / HY000`, per converted row |
| string negative value cast to unsigned | `1105 / HY000`, per converted row |

Diagnostics:

| Case | Behavior |
| --- | --- |
| missing default schema, unknown table, unknown column | existing descriptor SELECT diagnostics |
| unknown `USING` charset | `1115 / 42000` unknown character set |
| unsupported row operand | deterministic MyLite capability diagnostic |
| non-ASCII `latin1` row value | `CONVERT USING latin1 supports only ASCII scalar values` |
| unsupported syntax target or length form | existing syntax/capability diagnostic |
| physical SQLite callback failure | mapped through existing row-scalar physical failure handling |
| allocation failure | `MYLITE_NOMEM` with handle-owned diagnostic |

## MySQL 8.4.9 Runtime Observations

Observed against `mylite-mysql-849`:

- `CAST(s AS BINARY)`, `CONVERT(s, BINARY)`, and
  `CONVERT(s USING BINARY)` return the input text bytes with charset and
  collation `binary` and coercibility `2`.
- `CAST(n AS BINARY)` returns the decimal text bytes for numeric `n`.
- `CAST(n AS CHAR)` and `CONVERT(n, CHAR)` return decimal text using the
  connection charset/collation and coercibility `2`.
- `CAST(s AS SIGNED)` over row strings returns parsed integer prefixes, returns
  `0` for nonnumeric non-`NULL` strings, and emits warning 1292 for each
  truncated non-`NULL` string value.
- `CAST(nullable_string AS SIGNED)` returns `NULL` for `NULL` and `0` with
  warning 1292 for nonnumeric text.
- `CONVERT(s USING utf8)` reports charset `utf8mb3`, collation
  `utf8mb3_general_ci`, and warning 3719 once for the expression.
- `CONVERT(s USING utf8mb3)` reports charset `utf8mb3`, collation
  `utf8mb3_general_ci`, and warning 1287 once for the expression.
- `CONVERT(s USING latin1)` reports charset `latin1`, collation
  `latin1_swedish_ci`, and no warning for ASCII row values.
- Numeric signed-to-unsigned and unsigned-to-signed descriptor-column casts use
  numeric conversion semantics and do not emit the string complement warnings.

## Tests

Add MySQL-runtime expectation coverage and fast C runtime coverage for:

- row-backed binary, character, signed, unsigned, `USING BINARY`, and
  `USING` charset conversions over integer and nonbinary string descriptor
  columns;
- row-backed constants in table-backed `SELECT` conversion expressions;
- `NULL` propagation;
- target metadata collation ids for binary, `utf8mb4`, `utf8`/`utf8mb3`, and
  `latin1`;
- warning counts and warning rows for row string-to-integer truncation and
  `utf8`/`utf8mb3` charset names;
- unknown charset and unsupported row operands;
- existing scalar conversion unsupported forms remain unchanged outside
  row-backed `SELECT`;
- catalog generation, SQLite schema generation, and `.mylite` preamble remain
  unchanged by read-only conversion queries.
