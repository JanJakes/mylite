# Baseline TIME_FORMAT Function

## Goal

Add a narrow `TIME_FORMAT()` scalar-function slice for common temporal
projection SQL:

```sql
SELECT TIME_FORMAT(option_value, '%H.%i') FROM options;
```

This feature extends MyLite's scalar and single-table row-scalar projection
pipeline. It is not a general temporal expression engine, relaxed time parser,
locale subsystem, or full MySQL date/time formatting implementation.

## Sources

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing temporal and row-scalar designs:
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-time-second-conversion-functions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_time_format_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `TIME_FORMAT(time, format)` formats the first argument with the same
  percent-token model as `DATE_FORMAT()`, but the function is intended for
  hour, minute, second, and microsecond tokens.
- If either argument is `NULL`, the result is `NULL`.
- `TIME_FORMAT()` requires exactly two arguments. Wrong arities fail with
  `1582 / 42000`.
- MySQL accepts whitespace before `(` in default SQL mode and under
  `IGNORE_SPACE`. Unquoted `time_format` remains usable as an identifier in
  nonexpression contexts.
- A `DATETIME` or `TIMESTAMP` argument contributes its time part. A `DATE`
  column contributes `00:00:00` for non-`NULL` values.
- Invalid string inputs return `NULL` and append warning `1292 / 22007` with
  message `Truncated incorrect time value: 'value'`.
- Empty format strings return `NULL`.
- For hour values above `23`, `%H` and `%k` produce the full hour value; `%h`,
  `%I`, and `%l` use the hour modulo 12. For example,
  `TIME_FORMAT('100:00:00', '%H %k %h %I %l')` returns
  `100 100 04 04 4`.
- `TIME_FORMAT()` preserves a negative time by prefixing the whole formatted
  result with `-`; the sign is not repeated before each individual token.
- `%p` uses AM/PM after reducing hours to the clock day; `24:00:00` is AM and
  `12:00:00` is PM.
- Unknown percent sequences such as `%q`, `%%`, and a trailing `%` follow the
  current `DATE_FORMAT()` literal-output behavior.
- MySQL accepts date and week tokens but returns mixed `0` / `NULL` behavior.
  MyLite defers those tokens until broader date/week formatting semantics are
  implemented.
- Successful supported `SELECT` statements make a following `ROW_COUNT()`
  return `-1`; successful `DO` statements make it return `0`.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- `TIME_FORMAT(value, format)` with exactly two arguments;
- `value` as:
  - `NULL`;
  - a single- or double-quoted string literal containing canonical
    `[-]HH:MM:SS`, `[-]HHH:MM:SS`, or `YYYY-MM-DD HH:MM:SS`;
  - a descriptor column in a table-backed row-scalar `SELECT` whose current
    descriptor family is `TIME`, `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`,
    `VARCHAR`, or baseline `TEXT`;
- `format` as:
  - `NULL`;
  - a single- or double-quoted string literal without embedded `NUL`;
- format tokens:
  - `%H`, `%k`, `%h`, `%I`, `%l`, `%i`, `%S`, `%s`, `%T`, `%r`, `%p`, `%f`;
  - `%%`, a trailing `%`, and unknown non-format percent sequences such as
    `%q` as MySQL-style literal output;
- output text/`NULL` values through existing result APIs;
- warning count `0` for supported valid in-range forms.

Canonical time inputs may use signed hours up to the existing MyLite physical
`TIME` storage range. Fractional seconds are deferred; `%f` currently emits
`000000` for admitted inputs because this baseline stores no fractional
component.

## Deferred Surface

This slice intentionally does not support:

- date, day, month, year, weekday, ordinal, day-of-year, week, and week-year
  format tokens: `%Y`, `%y`, `%m`, `%c`, `%d`, `%e`, `%a`, `%W`, `%b`, `%M`,
  `%D`, `%j`, `%w`, `%U`, `%u`, `%V`, `%v`, `%X`, and `%x`;
- MySQL-compatible date-only string coercion for values such as
  `'2003-12-31'`. MyLite currently treats those values as invalid noncanonical
  time text and returns `NULL` with warning `1292`, rather than reproducing
  MySQL's quirky string-to-time coercion;
- relaxed time strings, numeric temporal inputs, incomplete values, trailing
  garbage truncation, fractional seconds, temporal offsets, locale effects, or
  SQL-mode-dependent temporal coercion beyond the explicit invalid string
  warning above;
- format columns, format expressions, variables, parameters, subqueries,
  `GET_FORMAT()`, `STR_TO_DATE()`, or arbitrary nesting;
- use in `WHERE`, `GROUP BY`, `HAVING`, DML assignments, defaults, generated
  columns, indexes, constraints, joins, CTEs, or arbitrary SQLite pass-through.

Deferred syntax or format-token behavior is rejected with deterministic MyLite
diagnostics instead of being approximated. Deferred relaxed input strings that
enter the admitted string-value envelope are handled as invalid temporal text
with warning `1292`.

## Grammar

MyLite adds this parser production:

```lemon
expression(A) ::= TIME_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
```

Wrong arities produce an argument-count AST node so runtime can return MySQL's
native-function parameter-count diagnostic:

```lemon
expression(A) ::= TIME_FORMAT(T) LPAREN RPAREN(R).
expression(A) ::= TIME_FORMAT(T) LPAREN expression(B) RPAREN(R).
expression(A) ::=
    TIME_FORMAT(T) LPAREN expression(B) COMMA expression(C)
    COMMA function_argument_list(D) RPAREN(R).
```

`TIME_FORMAT` is admitted as an identifier where MyLite admits ordinary
identifiers:

```lemon
identifier(A) ::= TIME_FORMAT(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
time_format_expr(A) ::= TIME_FORMAT LPAREN time_format_value COMMA time_format_format RPAREN.

time_format_value(A) ::= descriptor_time_column(B).
time_format_value(A) ::= descriptor_date_column(B).
time_format_value(A) ::= descriptor_datetime_column(B).
time_format_value(A) ::= descriptor_timestamp_column(B).
time_format_value(A) ::= descriptor_string_column(B).
time_format_value(A) ::= string_literal(T).
time_format_value(A) ::= NULL(T).

time_format_format(A) ::= string_literal(T).
time_format_format(A) ::= NULL(T).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projections that
   contain a top-level or parenthesized `TIME_FORMAT()` call.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literal arguments using the current statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Validate the format literal for this slice. Date and week tokens fail
   deterministically rather than returning wrong values.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. String literals and the descriptor input-kind marker
   are bound parameters.
7. Use a MyLite-owned SQLite scalar function for table-backed row execution so
   SQLite can keep scanning, filtering, ordering, and limiting without MyLite
   materializing source rows.

Scalar evaluation:

1. `NULL` value or `NULL` format returns `NULL`.
2. An empty format returns `NULL`.
3. `DATE` values contribute `00:00:00`.
4. `DATETIME` and `TIMESTAMP` values contribute their `HH:MM:SS` time part.
5. Valid signed time values are formatted according to the admitted token set.
6. Invalid non-`NULL` temporal text returns `NULL` and appends warning
   `1292 / 22007`, `Truncated incorrect time value: 'value'`.
7. Unknown non-format percent sequences such as `%q` output the character after
   `%`. A trailing `%` outputs `%`.

The row-backed generated SQL shape is:

```sql
_mylite_time_format(value_expr, format_expr, input_kind_expr)
```

`input_kind_expr` is a MyLite-owned string marker such as `time`, `date`,
`datetime`, `timestamp`, or `string`; it is not user-visible SQL.

## SQLite And Storage Boundary

This feature uses SQLite's public scalar-function registration API. It does not
change the SQLite fork, `.mylite` file format, VFS offset handling, catalog
descriptors, physical table layout, preamble, or storage invariants.

For table-backed projections SQLite still owns row scanning, predicate
filtering, ordering, and limiting. MyLite owns the parser nodes, descriptor
resolution, literal decoding, format validation, time parsing, warning
generation, and the scalar callback's MySQL-compatible formatting.

## Diagnostics

Supported MySQL-compatible diagnostics:

- wrong arity: `1582 / 42000`, native-function parameter-count error for
  `TIME_FORMAT`;
- unknown row column: existing `1054 / 42S22` unknown-column diagnostics;
- invalid non-`NULL` time input during execution: warning `1292 / 22007`,
  `Truncated incorrect time value: 'value'`.

MyLite-specific deterministic unsupported diagnostics:

- value argument outside the supported literal/descriptor family subset;
- format argument outside string literal or `NULL`;
- embedded `NUL` in admitted string literals;
- date/week format tokens deferred by this slice;
- MySQL-compatible date-only string literal coercion and broader relaxed
  temporal coercion.

Allocation failures propagate through the existing `MYLITE_NOMEM` diagnostic
path. Public API misuse behavior is unchanged because this feature adds no
public ABI.

## Tests

Fast C tests must cover:

- no-source, `FROM DUAL`, and `DO` evaluation;
- canonical time, negative time, long-hour time, datetime string, `NULL`, empty
  format, unknown percent tokens, and labels/whitespace;
- row-scalar projection over `TIME`, `DATE`, `DATETIME`, `TIMESTAMP`, and
  nonbinary string descriptor columns, including existing `WHERE`, `ORDER BY`,
  and `LIMIT`;
- warning behavior for invalid string inputs;
- wrong-arity, unknown-column, unsupported-value, unsupported-format,
  date-token, relaxed-date-string, and embedded-`NUL` diagnostics;
- identifier behavior for unquoted `time_format`;
- close/reopen persistence for descriptor rows used by row-scalar queries.

The MySQL expectation script verifies the same user-visible behavior against
MySQL 8.4.9 and records intentionally deferred MySQL-accepted cases.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-temporal.md`;
- `docs/compatibility/sql-query-expressions.md` and
  `docs/compatibility/type-system-literals-conversion.md` only if the support
  text needs to mention the added scalar/row-scalar literal surface.
