# Baseline CAST/CONVERT Basic Targets

## Summary

This phase extends the existing no-source scalar cast surface with a narrow set
of additional MySQL cast targets:

```sql
SELECT CAST(value AS CHAR)[, ...]
SELECT CAST(value AS CHAR(N) [CHARACTER SET charset] [BINARY])[, ...]
SELECT CAST(value AS DATE)[, ...]
SELECT CAST(value AS TIME|DATETIME|TIMESTAMP)[, ...]
SELECT CAST(value AS DECIMAL[(precision[, scale])])[, ...]
SELECT CAST(value AS JSON)[, ...]
SELECT CAST(value AS SIGNED [INTEGER|INT])[, ...]
SELECT CAST(value AS UNSIGNED [INTEGER|INT])[, ...]
SELECT CONVERT(value, CHAR)[, ...]
SELECT CONVERT(value, CHAR(N))[, ...]
SELECT CONVERT(value, DATE)[, ...]
SELECT CONVERT(value, TIME|DATETIME|TIMESTAMP)[, ...]
SELECT CONVERT(value, DECIMAL[(precision[, scale])])[, ...]
SELECT CONVERT(value, JSON)[, ...]
SELECT CONVERT(value, SIGNED [INTEGER|INT])[, ...]
SELECT CONVERT(value, UNSIGNED [INTEGER|INT])[, ...]
SELECT ... FROM DUAL
DO ...
```

The admitted `value` remains deliberately small: ordinary string literals,
decimal integer literals with optional unary sign, `TRUE`, `FALSE`, and `NULL`,
with optional parenthesization. This phase does not add table-backed casts,
DML-assignment casts, predicate casts, full temporal target semantics, full
decimal or approximate numeric target semantics, writable JSON/spatial casts,
parameters, subqueries, or arbitrary expression conversion. The scalar `DATE`,
`TIME`, `DATETIME`, `TIMESTAMP`, `DECIMAL`, and `JSON` targets are admitted as
compatibility placeholders with the limited result shaping described below.

The goal is to unlock the most common non-binary explicit cast targets while
preserving the current scalar-expression architecture and leaving table-backed
expression planning for a later, broader expression phase.

## Compatibility Authority

Primary references:

- MySQL 8.4 Reference Manual, "Cast Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

The manual documents `CAST(expr AS type)` and `CONVERT(expr, type)` as explicit
conversion forms and lists `CHAR`, temporal targets including `DATE`, `SIGNED
[INTEGER]`, and `UNSIGNED [INTEGER]` among supported targets. Runtime probes
verify that MySQL 8.4.9 also accepts `SIGNED INT` and `UNSIGNED INT` target
spellings after the signedness keyword, while bare `INT` / `INTEGER` are syntax
errors in cast target position.

Runtime probes captured in
`packages/libmylite/tests/mysql_baseline_cast_convert_basic_targets_expectations.sh`
establish this slice's expected behavior:

- `CAST(value AS CHAR)` and `CONVERT(value, CHAR)` return text for admitted
  string, integer, boolean, and `NULL` operands, with no warnings.
- `CAST(value AS BINARY(N))`, `CONVERT(value, BINARY(N))`, `CAST(value AS
  CHAR(N))`, and `CONVERT(value, CHAR(N))` truncate the current public scalar
  text result to the requested byte length and do not synthesize NUL padding.
- Character target clauses `CHAR(N) CHARACTER SET name [BINARY]`, `NCHAR(N)`,
  `NATIONAL CHAR(N)`, and `NATIONAL CHARACTER(N)` are accepted in scalar
  contexts and use the same byte-length result shaping.
- `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` targets are accepted in scalar
  contexts and extract canonical date/time/datetime text from canonical
  datetime string inputs.
- `DECIMAL[(precision[, scale])]` targets are accepted in scalar contexts and
  format the admitted scalar text input at the requested scale, defaulting to
  scale `0`.
- `FLOAT`, `REAL`, `DOUBLE`, and `DOUBLE PRECISION` targets are accepted as
  scalar placeholder targets and currently return the current public scalar text
  result unchanged.
- `JSON` targets are accepted in scalar contexts and normalize valid JSON text
  through MyLite's existing JSON normalizer.
- `CAST(value AS SIGNED)` / `CONVERT(value, SIGNED)` return signed `BIGINT`
  text for admitted operands.
- `CAST(value AS UNSIGNED)` / `CONVERT(value, UNSIGNED)` return unsigned
  `BIGINT` text for admitted operands.
- `SIGNED INTEGER`, `SIGNED INT`, `UNSIGNED INTEGER`, and `UNSIGNED INT` are
  accepted synonyms for the signed and unsigned targets.
- String-to-integer casts trim leading and trailing ASCII spaces, parse an
  optional leading sign and digit prefix, return `0` when no digits are
  usable, and emit MySQL warning `1292 / 22007` with message text
  `Truncated incorrect INTEGER value: '<input>'` when nonspace trailing text,
  malformed sign text, empty text, or out-of-range text is encountered.
- Exact positive string values greater than signed `BIGINT` max but within
  unsigned `BIGINT` range wrap to the signed two's-complement display for
  signed casts and emit warning `1105 / HY000` with MySQL's complement message.
- Negative string values cast to unsigned return the unsigned complement and
  emit the same `1105 / HY000` warning when the parsed magnitude is within the
  signed range.
- Out-of-range decimal integer literal operands use MySQL's `DECIMAL` warning
  wording where MySQL reports it; out-of-range string operands use `INTEGER`
  wording.
- `SELECT` leaves `ROW_COUNT() == -1`; `DO` leaves `ROW_COUNT() == 0`.
- Successful supported casts return no result set for `DO`, and no storage or
  catalog side effects.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Ownership Boundaries

- Public API: unchanged. Supported scalar `SELECT` statements return text or
  `NULL` through the existing `mylite_result` conventions. `DO` uses the
  existing non-row result conventions.
- Statement context and diagnostics: own statement-boundary warning publication,
  affected-row/row-count state, and warning count behavior. Cast conversion may
  stage warnings during scalar evaluation and publish them after all selected
  scalar cells have been evaluated.
- Lexer/parser/AST: admit only the target syntax described below and record
  source spans for labels and diagnostics.
- Runtime scalar evaluator: owns conversion for the current no-source,
  `FROM DUAL`, and `DO` scalar contexts. It must not rely on SQLite for
  MySQL-specific integer conversion or warning behavior.
- Catalog/analyzer/planner: unchanged. This feature does not resolve table
  descriptors, selected schemas, or row values.
- Result builder: appends source-span or explicit-alias labels and text/`NULL`
  scalar cells.
- Storage/VFS/file format: no storage writes, no `.mylite` preamble changes,
  and no shifted SQLite payload changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch.

## Supported SQL

```sql
cast_convert_item:
    cast_convert_scalar
  | cast_convert_scalar AS alias
  | cast_convert_scalar alias

cast_convert_scalar:
    CAST ( cast_convert_value AS cast_convert_target )
  | CONVERT ( cast_convert_value , cast_convert_target )
  | ( cast_convert_scalar )

cast_convert_target:
    CHAR
  | CHAR ( integer_literal ) [ CHARACTER SET charset_name ] [ BINARY ]
  | NCHAR [ ( integer_literal ) ]
  | NATIONAL CHAR [ ( integer_literal ) ]
  | NATIONAL CHARACTER [ ( integer_literal ) ]
  | DATE
  | TIME
  | DATETIME
  | TIMESTAMP
  | DECIMAL [ ( integer_literal [ , integer_literal ] ) ]
  | FLOAT [ ( integer_literal [ , integer_literal ] ) ]
  | REAL
  | DOUBLE [ PRECISION ]
  | JSON
  | SIGNED
  | SIGNED INTEGER
  | SIGNED INT
  | UNSIGNED
  | UNSIGNED INTEGER
  | UNSIGNED INT

cast_convert_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( cast_convert_value )
```

String literals use the existing MyLite string-literal decoding and SQL mode
policy. The current public scalar text surface still rejects decoded embedded
`NUL` bytes.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar with cast-target nodes:

```lemon
expression(A) ::= CAST(T) LPAREN expression(V) AS BINARY cast_length_opt(N) RPAREN(R).
expression(A) ::= CAST(T) LPAREN expression(V) AS cast_basic_target(C) RPAREN(R).
expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA BINARY cast_length_opt(N) RPAREN(R).
expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA cast_basic_target(C) RPAREN(R).

cast_basic_target(A) ::= CHAR(T) cast_length_opt(N) cast_character_set_opt(C) cast_binary_attribute_opt(B).
cast_basic_target(A) ::= DATE(T).
cast_basic_target(A) ::= TIME(T).
cast_basic_target(A) ::= DATETIME(T).
cast_basic_target(A) ::= TIMESTAMP(T).
cast_basic_target(A) ::= NCHAR(T) cast_length_opt(N).
cast_basic_target(A) ::= NATIONAL(T) CHAR(C) cast_length_opt(N).
cast_basic_target(A) ::= decimal_type_name(T) cast_decimal_precision_opt(P).
cast_basic_target(A) ::= SIGNED(T) cast_integer_name_opt(N).
cast_basic_target(A) ::= UNSIGNED(T) cast_integer_name_opt(N).

cast_integer_name_opt(A) ::= .
cast_integer_name_opt(A) ::= INTEGER(T).
cast_integer_name_opt(A) ::= INTEGER_TYPE(T). /* INT spelling only */
```

The existing `CONVERT(... USING ...)` productions remain separate. Bare
`INT`/`INTEGER` remain syntax errors in cast target position. Temporal,
decimal, approximate, and JSON targets in this slice are scalar placeholders,
not full MySQL type semantics.

## Runtime Semantics

`CAST(value AS CHAR)`, `CONVERT(value, CHAR)`, `CAST(value AS DATE)`, and
`CONVERT(value, DATE)`:

- return `NULL` when `value` is `NULL`;
- return decoded ordinary string literal text for string operands;
- return normalized decimal text for integer operands, preserving `-` only for
  negative nonzero values;
- return `1` or `0` for boolean operands;
- reject embedded `NUL` bytes through the existing scalar text diagnostic;
- emit no warnings for supported operands.

`SIGNED` and `UNSIGNED` targets:

- return `NULL` when `value` is `NULL`;
- convert boolean operands to `1` or `0`;
- parse string operands using MySQL-compatible integer prefix rules for this
  slice: skip ASCII whitespace, accept one optional sign, consume decimal
  digits, ignore trailing ASCII whitespace, and warn when no digits, malformed
  sign text, nonspace trailing text, or range clipping occurs;
- treat positive string magnitudes above signed `BIGINT` max but at or below
  unsigned `BIGINT` max as unsigned-width input for signed casts, producing the
  signed two's-complement display and warning with MySQL's complement warning;
- treat negative string magnitudes above signed `BIGINT` max as clipped to
  signed `BIGINT` min with an `INTEGER` truncation warning;
- treat unsigned casts of negative in-range input as unsigned complements with
  MySQL's complement warning;
- treat decimal integer literal operands independently from string operands so
  MySQL's observed `DECIMAL` truncation wording is preserved for out-of-range
  numeric literals;
- format signed results in signed decimal and unsigned results in unsigned
  decimal.

Integer conversion is MyLite-owned and bounded to at most one pass over the
input text. It does not materialize rows or call SQLite expression evaluation.

## Diagnostics

Supported warnings:

| Case | Warning |
| --- | --- |
| malformed/truncated string-to-integer input | `1292 / 22007`, `Truncated incorrect INTEGER value: '<input>'` |
| out-of-range decimal integer literal operand | `1292 / 22007`, `Truncated incorrect DECIMAL value: '<input>'` |
| positive unsigned-width input cast to signed | `1105 / HY000`, `Cast to signed converted positive out-of-range integer to its negative complement` |
| negative input cast to unsigned | `1105 / HY000`, `Cast to unsigned converted negative integer to its positive complement` |

Unsupported or rejected behavior:

| Case | Behavior |
| --- | --- |
| `CAST(value AS INT)` / `CAST(value AS INTEGER)` | MySQL-style syntax error |
| `CONVERT(value, INT)` / `CONVERT(value, INTEGER)` | MySQL-style syntax error |
| spatial targets | deterministic unsupported syntax for this phase |
| table-backed `SELECT CAST(column AS SIGNED) FROM t` | deterministic unsupported table-backed expression diagnostic |
| predicate, DML-assignment, default-expression, or generated-column casts | deterministic unsupported expression diagnostic |
| embedded `NUL` in scalar text operand | deterministic unsupported runtime error |
| allocation failure | `MYLITE_NOMEM` with the existing out-of-memory diagnostic |

## Tests

Add a MySQL expectation script and fast C tests covering:

- parser AST/source-span coverage for all admitted targets and target synonyms;
- MySQL syntax errors for bare `INT` / `INTEGER` targets;
- scalar `SELECT`, `FROM DUAL`, and `DO` result shape for `CHAR`, `DATE`,
  `SIGNED`, and `UNSIGNED` targets through both `CAST` and `CONVERT`;
- aliases and default expression labels;
- `NULL`, string, empty string, integer, signed integer, `TRUE`, and `FALSE`
  operands;
- string-to-integer prefix parsing, trailing-space acceptance, malformed text,
  and warning publication;
- signed and unsigned boundary values, complement behavior, and truncation
  diagnostics for string and decimal integer literal operands;
- scalar placeholder behavior for length-bearing binary/character targets,
  character-set target clauses, national-character targets, temporal targets,
  decimal scale formatting, approximate placeholders, and JSON normalization;
- deterministic rejection of unsupported table-backed casts, predicate casts,
  DML assignments, parameters, subqueries, and arbitrary expression operands;
- unchanged existing parser/scalar/cast/convert tests.

Verification before marking done:

1. `cmake --build --preset dev`
2. Focused parser/runtime scalar CTest entries.
3. `packages/libmylite/tests/mysql_baseline_cast_convert_basic_targets_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-casts.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for this exact
no-source/`DUAL`/`DO` subset. Do not claim table-backed casts, expression
assignments, predicate casts, full temporal cast semantics, full decimal/float
target semantics, spatial casts, protocol-grade metadata, or general expression
conversion.
