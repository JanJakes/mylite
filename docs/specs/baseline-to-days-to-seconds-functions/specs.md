# Baseline TO_DAYS And TO_SECONDS Functions

## Summary

This phase adds a narrow MySQL-compatible temporal conversion pair:

```sql
TO_DAYS(value)
TO_SECONDS(value)
```

The supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It does not add
general temporal expression coercion, predicates over function calls, expression
ordering, DML assignment values, generated columns, defaults, or arbitrary
nested expression planning.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing related MyLite specs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-calendar-date-functions/specs.md`
  - `docs/specs/baseline-datediff-function/specs.md`
  - `docs/specs/baseline-timestampdiff-function/specs.md`
  - `docs/specs/baseline-row-scalar-expression-projection/specs.md`
- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_to_days_to_seconds_functions_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 behavior, public SQLite
APIs, and existing MyLite code. It does not copy MySQL, MariaDB, Percona,
SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `TO_DAYS('2007-10-07')` returns `733321`.
- `TO_DAYS('2007-10-07 23:59:59')` returns `733321`; the time portion is
  ignored.
- `TO_DAYS('0000-01-01')` returns `1`; `TO_DAYS('0001-01-01')` returns `366`.
- `TO_DAYS('0000-00-00')`, `TO_DAYS('2001-11-00')`, and
  `TO_DAYS('2001-00-01')` return `NULL` and append warning `1292 / 22007` with
  text beginning `Incorrect datetime value:`.
- `TO_SECONDS('2009-11-29')` returns `63426672000`.
- `TO_SECONDS('2009-11-29 13:43:32')` returns `63426721412`.
- `TO_SECONDS('0000-01-01')` returns `86400`; `TO_SECONDS('0001-01-01')`
  returns `31622400`.
- `TO_SECONDS('0000-00-00')`, partial-zero dates, and invalid datetime strings
  return `NULL` and append warning `1292 / 22007` with text beginning
  `Incorrect datetime value:`.
- Invalid datetime time fields such as `24:00:00` return `NULL` and warn with
  the same `Incorrect datetime value:` class.
- `NULL` arguments return `NULL` without warnings.
- Stored `DATE`, `DATETIME`, and `TIMESTAMP` descriptor values that contain
  zero or partial-zero dates return `NULL` without appending a new warning.
  String descriptor values containing invalid text warn like string literals.
- MySQL accepts broader inputs such as numeric dates, `T` separators,
  fractional seconds, and warning-producing trailing text. This slice defers
  those forms because they belong to the broader temporal coercion model.
- The function names accept whitespace before `(` in default SQL mode.
- The function names remain usable as unquoted identifiers outside function
  calls.
- `ANSI_QUOTES` makes double-quoted arguments identifiers rather than string
  literals.
- Wrong argument counts fail with native-function argument-count diagnostic
  `1582 / 42000`.
- Successful supported statements produce no warnings. A successful scalar
  `SELECT` makes a following `ROW_COUNT()` return `-1`; a successful `DO`
  makes it return `0`.

## Supported SQL

No-source and `DUAL` projection:

```sql
SELECT TO_DAYS(date_value)
SELECT TO_SECONDS(datetime_value)
SELECT TO_DAYS(date_value), TO_SECONDS(datetime_value) FROM DUAL
```

`DO` execution:

```sql
DO TO_DAYS(date_value), TO_SECONDS(datetime_value)
```

Single-table row-scalar projection:

```sql
SELECT TO_DAYS(column_name), TO_SECONDS(column_name)
FROM table_name [WHERE predicate] [ORDER BY column_name [ASC | DESC]] [LIMIT row_count]
```

The existing single-table row-scalar envelope supplies target table resolution,
predicate resolution, descriptor-column projection, `ORDER BY`, and `LIMIT`
behavior.

### Inputs

`date_value` / `datetime_value` is one of:

- `NULL`;
- a single- or double-quoted string literal in one of these exact forms:
  - `YYYY-MM-DD`;
  - `YYYY-MM-DD HH:MM:SS`;
- a row-scalar descriptor column of type `DATE`, `DATETIME`, `TIMESTAMP`,
  `CHAR`, `VARCHAR`, or baseline `TEXT`.

`TO_DAYS()` ignores any admitted time part. `TO_SECONDS()` treats a date-only
argument as midnight.

### Results

`TO_DAYS()` returns the MySQL day number for a complete admitted date. The
calculation treats year `0000` as a non-leap year, matching verified MySQL
8.4.9 behavior for the supported range.

`TO_SECONDS()` returns `TO_DAYS(date) * 86400 + hour * 3600 + minute * 60 +
second` for complete admitted date/datetime values. The result fits signed
64-bit storage for the supported MySQL year range.

Invalid non-`NULL` string values return `NULL` and append warning `1292`.
Invalid non-string descriptor values return `NULL` without a new warning,
matching verified MySQL row-backed behavior for stored zero temporals.

## Deferred Surface

This slice intentionally does not support:

- numeric temporal literals, booleans, bit/hex literals, decimal or float
  values, parameters, variables, subqueries, or arbitrary expressions as
  arguments;
- compact numeric temporal text, `T` separators, fractional seconds, trailing
  text, time-zone suffixes, two-digit years, locale coercion, or broader
  SQL-mode-sensitive temporal parsing;
- `TIME` descriptor arguments;
- `TO_DAYS()` / `TO_SECONDS()` inside `WHERE`, `ORDER BY`, `GROUP BY`,
  `HAVING`, DML assignments, defaults, generated columns, indexes,
  constraints, joins, CTEs, or arbitrary SQLite pass-through;
- protocol metadata parity beyond existing result-object conventions.

## Grammar

MyLite adds these independently authored Lemon-shape productions:

```lemon
expression(A) ::= TO_DAYS(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= TO_SECONDS(T) LPAREN expression(V) RPAREN(R).
```

Unsupported arities are represented explicitly so the runtime can report the
verified native-function argument-count diagnostic:

```lemon
expression(A) ::= TO_DAYS(T) LPAREN RPAREN(R).
expression(A) ::= TO_DAYS(T) LPAREN expression(V) COMMA function_argument_list(L) RPAREN(R).
expression(A) ::= TO_SECONDS(T) LPAREN RPAREN(R).
expression(A) ::= TO_SECONDS(T) LPAREN expression(V) COMMA function_argument_list(L) RPAREN(R).
```

The function names remain admitted as ordinary keyword identifiers where the
existing grammar accepts nonreserved keyword identifiers:

```lemon
identifier(A) ::= TO_DAYS(T).
identifier(A) ::= TO_SECONDS(T).
```

Semantic narrowing is enforced by the analyzer/runtime:

```lemon
to_days_expr ::= TO_DAYS LPAREN supported_date_value RPAREN.
to_seconds_expr ::= TO_SECONDS LPAREN supported_datetime_value RPAREN.

supported_date_value ::= NULL.
supported_date_value ::= canonical_date_string.
supported_date_value ::= canonical_datetime_string.
supported_date_value ::= descriptor_date_column.
supported_date_value ::= descriptor_datetime_column.
supported_date_value ::= descriptor_timestamp_column.
supported_date_value ::= descriptor_nonbinary_string_column.

supported_datetime_value ::= supported_date_value.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts containing the admitted functions.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literals using the current statement SQL mode, including
   `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Reject unsupported argument kinds before generated SQLite SQL exists.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. String literals and internal function/input-kind
   discriminators are bound parameters.
7. Use the existing MyLite-owned SQLite temporal scalar function for table-
   backed row execution.

Evaluation:

- `NULL` input returns `NULL`.
- Complete date values use the proleptic Gregorian calendar and MySQL's day
  numbering used by the current `DATEDIFF()`/calendar-date function helpers.
- Complete datetime values must have time fields `00:00:00` through `23:59:59`.
- Month zero, day zero, full-zero dates, and invalid calendar dates are invalid
  for this function pair. String inputs warn; descriptor temporal inputs return
  `NULL` without adding a warning when invalid stored values are encountered.
- Supported in-range calls produce no warnings.

SQLite integration uses public `sqlite3_create_function_v2()` scalar callbacks
registered by MyLite. No SQLite fork patch is needed.

## Diagnostics

- Wrong argument count: MySQL-compatible native function parameter-count error
  `1582 / 42000`.
- Unknown no-source identifiers: existing unknown-column diagnostic.
- Unsupported scalar argument kind: deterministic MyLite unsupported diagnostic.
- Unsupported row-backed descriptor kind: deterministic MyLite unsupported
  diagnostic.
- Invalid string temporal values: warning `1292 / 22007` and `NULL` result.
- String literals containing embedded `NUL`: deterministic unsupported
  diagnostic.
- Allocation failure: existing `MYLITE_NOMEM` diagnostic.

No public API changes are introduced.

## Tests

Fast C tests cover:

- no-source, `DUAL`, and `DO` successful execution;
- `ROW_COUNT()` and `@@warning_count` after supported statements;
- `NULL`, complete year-zero dates, pre-1000 dates, ordinary dates, datetimes,
  leap-day and non-leap-day behavior;
- invalid full-zero and partial-zero string warnings;
- invalid string descriptor warnings and stored zero-temporal descriptor
  no-warning behavior;
- table-backed row-scalar projection over `DATE`, `DATETIME`, `TIMESTAMP`, and
  string-family descriptor columns with `WHERE`, `ORDER BY`, `LIMIT`, and
  reopen persistence;
- wrong arities, unsupported literal/expression forms, unsupported descriptor
  families, unknown columns, and `ANSI_QUOTES` identifier behavior;
- identifier usability for `to_days` / `to_seconds` table names.

MySQL expectation tests verify each user-visible behavior admitted by this
slice against MySQL 8.4.9.
