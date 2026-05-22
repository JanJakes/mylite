# Baseline EXTRACT Function

## Summary

This phase adds a narrow MySQL-compatible `EXTRACT(unit FROM expr)` temporal
function slice. The supported surface covers no-source scalar `SELECT`,
`SELECT ... FROM DUAL`, `DO`, and single-table row-scalar projection contexts
already used by the existing temporal function batches.

The goal is to cover the common calendar and time units that can be implemented
with MyLite's existing canonical temporal parser:

```sql
EXTRACT(YEAR FROM expr)
EXTRACT(QUARTER FROM expr)
EXTRACT(MONTH FROM expr)
EXTRACT(DAY FROM expr)
EXTRACT(HOUR FROM expr)
EXTRACT(MINUTE FROM expr)
EXTRACT(SECOND FROM expr)
EXTRACT(YEAR_MONTH FROM expr)
EXTRACT(DAY_HOUR FROM expr)
EXTRACT(DAY_MINUTE FROM expr)
EXTRACT(DAY_SECOND FROM expr)
EXTRACT(HOUR_MINUTE FROM expr)
EXTRACT(HOUR_SECOND FROM expr)
EXTRACT(MINUTE_SECOND FROM expr)
```

`MICROSECOND`, `WEEK`, and microsecond composite units are parsed only far
enough to reject them deterministically as unsupported by this baseline.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing temporal expression specifications:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-date-type/specs.md`
  - `docs/specs/baseline-time-type/specs.md`
  - `docs/specs/baseline-datetime-type/specs.md`
  - `docs/specs/baseline-timestamp-type/specs.md`
- Official MySQL 8.4 Reference Manual:
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
  - temporal intervals:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - date and time literals:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_extract_function_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `EXTRACT(YEAR|MONTH|DAY FROM datetime)` returns the corresponding calendar
  part as an integer.
- `EXTRACT(QUARTER FROM datetime)` returns `1` through `4`, and returns `0`
  for a zero month.
- `EXTRACT(HOUR|MINUTE|SECOND FROM datetime)` returns the corresponding time
  part.
- `EXTRACT(HOUR|MINUTE|SECOND FROM negative_time)` preserves the sign on the
  returned part. This differs from `HOUR()`, `MINUTE()`, and `SECOND()`, which
  return the absolute component for the current MySQL-runtime-verified slice.
- Composite units concatenate fixed-width right-hand components into an integer:
  `YEAR_MONTH = year * 100 + month`,
  `DAY_HOUR = day * 100 + hour`,
  `DAY_MINUTE = day * 10000 + hour * 100 + minute`,
  `DAY_SECOND = day * 1000000 + hour * 10000 + minute * 100 + second`,
  `HOUR_MINUTE = hour * 100 + minute`,
  `HOUR_SECOND = hour * 10000 + minute * 100 + second`, and
  `MINUTE_SECOND = minute * 100 + second`.
- Negative `TIME` values make supported time and day-time composite units
  negative.
- `NULL` input returns `NULL`.
- Full-zero and partial-zero date strings are accepted by supported date units
  in empty SQL mode where the existing temporal extractor accepts them.
- Invalid date-unit input returns `NULL` with warning `1292` beginning
  `Incorrect datetime value:`.
- Invalid time-unit and day-time composite input returns `NULL` with warning
  `1292` beginning `Truncated incorrect time value:`.
- Successful supported calls produce no warnings. A scalar `SELECT` makes
  `ROW_COUNT()` return `-1`; a supported `DO` makes it return `0`.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT EXTRACT(extract_unit FROM temporal_value)[, ...]
SELECT EXTRACT(extract_unit FROM temporal_value)[, ...] FROM DUAL
```

`DO` form:

```sql
DO EXTRACT(extract_unit FROM temporal_value)[, ...]
```

Single-table row-backed forms, with at least one select item containing a
supported row-scalar `EXTRACT()` call:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

Supported `temporal_value` forms are:

- `NULL`;
- a single- or double-quoted string literal in a currently supported canonical
  temporal shape;
- a descriptor column in a table-backed row-scalar `SELECT`.

Date-unit arguments accept canonical date or datetime values and descriptor
families already admitted by the existing date-part temporal extractor:
`DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, and baseline `TEXT`.

Time-unit and day-time composite arguments accept canonical time or datetime
values and descriptor families already admitted by the existing time-part
temporal extractor: `TIME`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, and
baseline `TEXT`.

Descriptor `DATE` inputs to time/day-time units and descriptor `TIME` inputs to
date units are deferred in this baseline even where MySQL accepts broader
temporal coercions.

`YEAR_MONTH` and `QUARTER` are date units. `DAY_HOUR`, `DAY_MINUTE`,
`DAY_SECOND`, `HOUR_MINUTE`, `HOUR_SECOND`, and `MINUTE_SECOND` are time-capable
units. For datetime inputs, the `DAY_*` units use the calendar day-of-month.
For time inputs, the day component is zero and the sign comes from the time.

## Deferred Surface

This slice intentionally does not support:

- `MICROSECOND`, `WEEK`, `DAY_MICROSECOND`, `HOUR_MICROSECOND`,
  `MINUTE_MICROSECOND`, or `SECOND_MICROSECOND`;
- fractional seconds;
- numeric temporal literals, boolean values, bit/hex literals, decimal or float
  values, parameters, variables as arguments, subqueries, or arbitrary
  expressions;
- date-only strings in time-unit extraction and time-only strings in date-unit
  extraction beyond the existing invalid-value warning behavior;
- descriptor `DATE` inputs to time/day-time units and descriptor `TIME` inputs
  to date units;
- relaxed temporal strings, two-digit year coercion, compact numeric temporal
  text, locale or time-zone coercion, or broader SQL-mode-sensitive temporal
  parsing;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through.

## Grammar

MyLite adds these parser productions:

```lemon
expression(A) ::= EXTRACT(T) LPAREN extract_unit(U) FROM expression(V) RPAREN(R).

extract_unit(A) ::= YEAR(T).
extract_unit(A) ::= QUARTER(T).
extract_unit(A) ::= MONTH(T).
extract_unit(A) ::= DAY(T).
extract_unit(A) ::= HOUR(T).
extract_unit(A) ::= MINUTE(T).
extract_unit(A) ::= SECOND(T).
extract_unit(A) ::= MICROSECOND(T).
extract_unit(A) ::= WEEK(T).
extract_unit(A) ::= YEAR_MONTH(T).
extract_unit(A) ::= DAY_HOUR(T).
extract_unit(A) ::= DAY_MINUTE(T).
extract_unit(A) ::= DAY_SECOND(T).
extract_unit(A) ::= HOUR_MINUTE(T).
extract_unit(A) ::= HOUR_SECOND(T).
extract_unit(A) ::= MINUTE_SECOND(T).
extract_unit(A) ::= DAY_MICROSECOND(T).
extract_unit(A) ::= HOUR_MICROSECOND(T).
extract_unit(A) ::= MINUTE_MICROSECOND(T).
extract_unit(A) ::= SECOND_MICROSECOND(T).
```

The nonreserved unit/function words remain usable as unquoted identifiers where
MyLite admits ordinary MySQL nonreserved keywords.

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts containing `EXTRACT()`.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literal arguments using the current statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Reject unsupported units before generated SQLite SQL is built.
6. Reject unsupported argument kinds before generated SQLite SQL is built.
7. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. Literal arguments and MyLite-internal function
   discriminators are bound parameters.
8. Use the existing MyLite-owned SQLite scalar callback path for table-backed
   row execution so SQLite keeps scanning, filtering, ordering, and limiting
   without MyLite materializing source rows.

Scalar evaluation:

1. `NULL` input returns `NULL`.
2. Supported date units parse canonical date or datetime input and return a
   decimal integer result.
3. Supported time units parse canonical time or datetime input and return a
   decimal integer result. Negative time values produce negative results for
   `EXTRACT()` time and day-time units.
4. Invalid date-unit input returns `NULL` and appends warning `1292`.
5. Invalid time-unit input returns `NULL` and appends warning `1292`.

SQLite integration is a MyLite wrapper/translation plus public SQLite scalar
function callback. This phase does not require SQLite fork changes.

## Diagnostics

- Syntax errors use the existing parser diagnostics.
- Unsupported units use deterministic MyLite unsupported diagnostics naming the
  unsupported `EXTRACT()` unit.
- Unsupported argument kinds use deterministic MyLite unsupported diagnostics
  for `EXTRACT()` arguments.
- Unknown row-backed columns reuse the existing unknown-column diagnostics.
- Invalid date/time values reuse the existing warning `1292` pathways.
- Allocation failure returns `MYLITE_NOMEM`.

## Tests

Tests must cover:

- no-source and `DUAL` scalar calls for all supported units;
- signed negative `TIME` behavior for simple and composite time units;
- `NULL`, zero-date, partial-zero date, invalid date values, invalid time
  values, warnings, `ROW_COUNT()`, and `@@warning_count`;
- row-backed descriptor columns for `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`,
  and string-family input;
- filtered, ordered, limited row-scalar execution staying in SQLite's scan path;
- reopen persistence of source rows after `EXTRACT()` projection;
- unsupported units and unsupported argument shapes;
- parser coverage for the grammar and identifier reuse.
