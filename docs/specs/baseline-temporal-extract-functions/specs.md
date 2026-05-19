# Baseline Temporal Extract Functions

## Summary

This phase adds a narrow MySQL-compatible temporal extraction batch:

```sql
DATE(expr)
YEAR(expr)
MONTH(expr)
DAY(expr)
DAYOFMONTH(expr)
HOUR(expr)
MINUTE(expr)
SECOND(expr)
```

The supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It is not a
general temporal expression engine and does not add expression predicates,
expression ordering, DML assignment values, generated columns, defaults, casts,
or arbitrary nested expression planning.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing temporal and row-scalar expression designs:
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-date-add-second/specs.md`
  - `docs/specs/baseline-current-date-time-functions/specs.md`
  - `docs/specs/baseline-date-type/specs.md`
  - `docs/specs/baseline-time-type/specs.md`
  - `docs/specs/baseline-datetime-type/specs.md`
  - `docs/specs/baseline-timestamp-type/specs.md`
  - `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
- Official MySQL 8.4 Reference Manual:
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
  - date and time literals:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html>
  - function-name parsing and resolution:
    <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_temporal_extract_functions_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `DATE(datetime)` returns the date portion as `YYYY-MM-DD`.
- `DATE(date)` returns the same date text.
- `YEAR()`, `MONTH()`, `DAY()`, and `DAYOFMONTH()` return integer date parts.
  `DAY()` is a synonym for `DAYOFMONTH()`.
- `HOUR()`, `MINUTE()`, and `SECOND()` return integer time parts from time or
  datetime values.
- If the argument is `NULL`, the result is `NULL`.
- Full-zero, partial-zero, and year-zero date strings are accepted by date-part
  extractors in empty SQL mode. `DATE('0000-00-00')` returns `0000-00-00`;
  `DATE('0000-01-02')` returns `0000-01-02`; `YEAR()` / `MONTH()` / `DAY()`
  return numeric zero parts as `0`.
- `HOUR()`, `MINUTE()`, and `SECOND()` preserve the absolute time components
  for negative time strings such as `'-13:29:17'`.
- `HOUR()` accepts hour values beyond `23` for time strings, matching MySQL
  `TIME` display range behavior.
- Invalid date input for `DATE()`, `YEAR()`, `MONTH()`, `DAY()`, and
  `DAYOFMONTH()` returns `NULL` and records warning `1292` with text beginning
  `Incorrect datetime value:`.
- Invalid time input for `HOUR()`, `MINUTE()`, and `SECOND()` returns `NULL`
  and records warning `1292` with text beginning
  `Truncated incorrect time value:`.
- MySQL accepts broader numeric temporal literals, fractional seconds, relaxed
  date/time strings, and date-only strings passed to time extractors. This
  slice defers those forms because they require MySQL's full temporal coercion
  and warning model.
- Whitespace between the function name and `(` is accepted for these functions
  in empty SQL mode.
- Successful supported calls produce no warnings. A preceding scalar `SELECT`
  makes `ROW_COUNT()` return `-1`; a supported `DO` makes it return `0`.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT temporal_extract_item[, temporal_extract_item ...]
SELECT temporal_extract_item[, temporal_extract_item ...] FROM DUAL
```

`DO` form:

```sql
DO temporal_extract_expr[, temporal_extract_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
temporal extractor:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted function expression shape is:

```sql
temporal_extract_expr:
    DATE ( date_value )
  | YEAR ( date_value )
  | MONTH ( date_value )
  | DAY ( date_value )
  | DAYOFMONTH ( date_value )
  | HOUR ( time_value )
  | MINUTE ( time_value )
  | SECOND ( time_value )
```

`date_value` is one of:

- `NULL`;
- a single- or double-quoted string literal containing one of:
  - `YYYY-MM-DD`;
  - `YYYY-MM-DD HH:MM:SS`;
  - `0000-00-00`;
  - year-zero forms such as `0000-01-02`;
  - `YYYY-00-00`;
  - `YYYY-MM-00`;
  - the same date forms with a `HH:MM:SS` time part;
- a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
  family is `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline
  `TEXT`.

`time_value` is one of:

- `NULL`;
- a single- or double-quoted string literal containing one of:
  - `HH:MM:SS`;
  - `-HH:MM:SS`;
  - `HHH:MM:SS`;
  - `YYYY-MM-DD HH:MM:SS`, including zero-date forms with a time part;
- a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
  family is `TIME`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline
  `TEXT`.

The resulting value is text for `DATE()` and signed integer/`NULL` for the
other extractor functions. The public result object uses existing scalar and
row-result conventions.

## Deferred Surface

This slice intentionally does not support:

- numeric temporal literals, boolean values, bit/hex literals, decimal or float
  values, parameters, variables as arguments, subqueries, or arbitrary
  expressions;
- fractional seconds;
- relaxed temporal strings, two-digit year coercion, compact numeric temporal
  text, locale or time-zone coercion, and general SQL-mode-sensitive temporal
  parsing beyond the zero-temporal strings explicitly admitted above;
- date-only inputs to `HOUR()`, `MINUTE()`, or `SECOND()`, because MySQL parses
  them through warning-producing time coercion that is outside this batch;
- `TIME` inputs to `DATE()`, `YEAR()`, `MONTH()`, `DAY()`, or `DAYOFMONTH()`;
- `DAYNAME()`, `DAYOFWEEK()`, `DAYOFYEAR()`, `EXTRACT()`, `MICROSECOND()`,
  `QUARTER()`, `WEEK()`, `WEEKDAY()`, `WEEKOFYEAR()`, `YEARWEEK()`, or other
  temporal functions outside later feature slices such as
  `docs/specs/baseline-time-function/specs.md`;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through.

## Grammar

MyLite adds these parser productions:

```lemon
expression(A) ::= DATE(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= YEAR(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= MONTH(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= DAY(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= DAYOFMONTH(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= HOUR(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= MINUTE(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= SECOND(T) LPAREN expression(V) RPAREN(R).
```

The function names remain usable as unquoted identifiers where MyLite admits
ordinary MySQL nonreserved keywords:

```lemon
identifier(A) ::= DATE(T).
identifier(A) ::= YEAR(T).
identifier(A) ::= MONTH(T).
identifier(A) ::= DAY(T).
identifier(A) ::= DAYOFMONTH(T).
identifier(A) ::= HOUR(T).
identifier(A) ::= MINUTE(T).
identifier(A) ::= SECOND(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
date_extract_expr(A) ::= date_extract_name(T) LPAREN supported_date_value(V) RPAREN.
time_extract_expr(A) ::= time_extract_name(T) LPAREN supported_time_value(V) RPAREN.

date_extract_name ::= DATE | YEAR | MONTH | DAY | DAYOFMONTH.
time_extract_name ::= HOUR | MINUTE | SECOND.

supported_date_value ::= descriptor_date_column.
supported_date_value ::= descriptor_datetime_column.
supported_date_value ::= descriptor_timestamp_column.
supported_date_value ::= descriptor_string_column.
supported_date_value ::= string_literal.
supported_date_value ::= NULL.

supported_time_value ::= descriptor_time_column.
supported_time_value ::= descriptor_datetime_column.
supported_time_value ::= descriptor_timestamp_column.
supported_time_value ::= descriptor_string_column.
supported_time_value ::= string_literal.
supported_time_value ::= NULL.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts that contain a top-level or parenthesized temporal extractor call.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literal arguments using the current statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Reject unsupported argument kinds before generated SQLite SQL is built.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. Literal arguments and MyLite-internal function
   discriminators are bound parameters.
7. Use a MyLite-owned SQLite scalar function for table-backed row execution so
   SQLite can keep scanning, filtering, ordering, and limiting without MyLite
   materializing source rows.

Scalar evaluation:

1. `NULL` input returns `NULL`.
2. `DATE()` returns the canonical date portion for date and datetime values.
3. `YEAR()`, `MONTH()`, `DAY()`, and `DAYOFMONTH()` return the corresponding
   date part as an integer. Zero date parts return `0`.
4. `HOUR()`, `MINUTE()`, and `SECOND()` return the corresponding time part as
   an integer. Negative time values return the absolute component values, as
   observed in MySQL for this slice.
5. Invalid non-`NULL` date input returns `NULL` and appends warning
   `1292 / 22007`, `Incorrect datetime value: 'value'`.
6. Invalid non-`NULL` time input returns `NULL` and appends warning
   `1292 / 22007`, `Truncated incorrect time value: 'value'`.
7. String literal shapes outside the admitted temporal text forms are accepted
   syntactically and then handled as invalid date or time inputs for this
   slice, rather than invoking broader MySQL temporal coercion.

The row-backed generated SQL shape is:

```sql
_mylite_temporal_extract(value_expr, extract_kind_expr, input_kind_expr)
```

`extract_kind_expr` is a bound MyLite-internal discriminator such as `date`,
`year`, or `hour`. `input_kind_expr` is a bound MyLite-internal discriminator
such as `string`, `date`, `time`, `datetime`, or `timestamp`. Neither value is
user-visible.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through existing `mylite_execute()`
  and result APIs.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization.
- Lexer/parser/AST: admits the function names and preserves source spans for
  labels and diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated SQLite
  SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers an internal scalar function through SQLite's public
  function API for extraction semantics. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Diagnostics

Supported valid in-range calls produce `warning_count == 0`.

Diagnostics for this phase:

- syntax errors or malformed arity forms: existing parser syntax diagnostics,
  except forms already represented as native-function argument count errors by
  the surrounding parser;
- unsupported scalar argument kind:
  `temporal extract functions support only string temporal literals and NULL`
  or a function-specific variant;
- unsupported row argument kind:
  `row-scalar SELECT temporal extract functions support only descriptor columns`;
- unsupported descriptor family:
  `temporal extract functions support only DATE, TIME, DATETIME, TIMESTAMP, string, and NULL values`;
- string literal contains embedded `NUL`:
  `temporal extract literals do not support NUL bytes`;
- invalid date input warning:
  `1292 / 22007`, `Incorrect datetime value: 'value'`;
- invalid time input warning:
  `1292 / 22007`, `Truncated incorrect time value: 'value'`;
- allocation, SQLite preparation, SQLite stepping, or result construction
  failures use existing runtime diagnostics.

## Tests

Add:

- MySQL-runtime expectation script:
  `packages/libmylite/tests/mysql_baseline_temporal_extract_functions_expectations.sh`;
- parser coverage for admitted function calls, function-name whitespace,
  aliases, identifier use, and unsupported shapes;
- runtime coverage for:
  - no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
  - labels, aliases, row count, warning count, and absence of rows for `DO`;
  - `NULL` propagation;
  - canonical date, datetime, time, zero-date, and partial-zero date strings;
  - descriptor-backed `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, and supported
    string columns;
  - invalid date/time string warnings;
  - existing `WHERE`, `ORDER BY`, and `LIMIT` row envelope preservation;
  - reopen persistence readback through temporal extractors;
  - unsupported numeric and expression argument forms plus unsupported
    descriptor-family forms;
  - MySQL-runtime evidence for deferred numeric temporal literals, fractional
    temporal strings, and date-only-to-time extractor coercion;
  - zero-initialized cleanup for any new planner/result objects.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-temporal.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/type-system-literals-conversion.md` only if the admitted
  literal surface changes beyond existing documented temporal strings.

Use limited wording. Do not claim full temporal expression support, full MySQL
temporal coercion, fractional seconds, time zones, expression predicates,
expression ordering, DML assignment support, generated columns, defaults, or
the deferred temporal functions.
