# Baseline Day And Month Name Functions

## Goal

Add a narrow calendar-name function batch:

```sql
DAYNAME(expr)
MONTHNAME(expr)
```

This phase extends MyLite's existing no-source scalar, `DUAL`, `DO`, and
single-table row-scalar projection paths. It is not a general temporal
expression engine and does not add expression predicates, expression ordering,
DML assignment values, generated columns, defaults, casts, locale variables, or
arbitrary nested expression planning.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, MySQL server locale support:
  <https://dev.mysql.com/doc/refman/8.4/en/locale-support.html>
- Existing MyLite temporal designs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-calendar-date-functions/specs.md`
  - `docs/specs/baseline-week-temporal-functions/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_day_month_name_functions_expectations.sh`.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `DAYNAME(date)` returns the full weekday name for a complete calendar date.
- `MONTHNAME(date)` returns the full month name when the month portion is
  nonzero and valid.
- The default MySQL `lc_time_names` value is `en_US`, and that variable changes
  day and month names. MyLite does not yet expose `lc_time_names`, so this slice
  returns English `en_US` names only.
- Datetime arguments use their date portion when the time fields are valid
  `00:00:00` through `23:59:59`.
- `NULL` arguments make the result `NULL` without warnings.
- Pre-1000 canonical dates such as `'0001-01-01'` and `'0999-12-31'`
  are accepted for these functions without warnings.
- Complete year-zero dates such as `'0000-01-02'` are accepted. Year zero is
  not treated as a leap year.
- `DAYNAME()` rejects full-zero, month-zero, day-zero, and otherwise invalid
  dates, returns `NULL`, and appends warning `1292 / 22007` with text beginning
  `Incorrect datetime value:`.
- `MONTHNAME()` rejects full-zero and month-zero dates, but accepts a zero day
  when the year and month otherwise identify a valid month:
  `MONTHNAME('2001-11-00')` returns `November` without a warning from
  `MONTHNAME()` itself.
- `MONTHNAME()` rejects invalid nonzero days, including year-zero leap-day
  values such as `'0000-02-29'`.
- Datetime strings with invalid time fields such as `24:00:00` or `99:00:00`
  return `NULL` and append warning `1292 / 22007`.
- Stored `DATE`, `DATETIME`, and `TIMESTAMP` descriptor values that are
  full-zero or partial-zero return `NULL` or the accepted `MONTHNAME()` value
  without adding a new warning; string descriptor values containing the same
  invalid text warn like string literals.
- Non-temporal strings return `NULL` and warn with `1292 / 22007`.
- MySQL accepts broader numeric temporal values, boolean coercion, `TIME`
  coercions, trailing delimiters, and some ISO-like strings with
  warning-producing truncation. This slice defers those coercions because they
  belong to the broader temporal conversion model.
- The function names accept whitespace before `(` in default SQL mode.
- The function names remain usable as unquoted identifiers outside function
  calls.
- `ANSI_QUOTES` makes double-quoted arguments identifiers rather than string
  literals. `NO_BACKSLASH_ESCAPES` leaves ordinary quoted date strings usable.
- Wrong argument counts fail with `1582 / 42000`, native function
  parameter-count diagnostics.
- Successful supported statements produce no warnings. A successful scalar
  `SELECT` makes a following `ROW_COUNT()` return `-1`; a successful `DO`
  makes it return `0`.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT calendar_name_item[, calendar_name_item ...]
SELECT calendar_name_item[, calendar_name_item ...] FROM DUAL
```

`DO` form:

```sql
DO calendar_name_expr[, calendar_name_expr ...]
```

Single-table row-backed forms, with at least one select item containing one of
the admitted functions:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted function expression shape is:

```sql
calendar_name_expr:
    DAYNAME ( date_value )
  | MONTHNAME ( date_value )
```

`date_value` is one of:

- `NULL`;
- a single- or double-quoted string literal containing canonical `YYYY-MM-DD`
  or `YYYY-MM-DD HH:MM:SS`, including pre-1000 years;
- the same canonical shape with complete year-zero dates;
- for `MONTHNAME()` only, the same shape with `DD = 00` when `YYYY` and `MM`
  otherwise identify a valid month;
- a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
  family is `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline
  `TEXT`.

The resulting value is English text or SQL `NULL`. The public result object uses
existing scalar and row-result conventions.

## Deferred Surface

This slice intentionally does not support:

- locale-sensitive output or `lc_time_names` system-variable behavior;
- abbreviated day or month names beyond existing `DATE_FORMAT()` support;
- numeric temporal literals, booleans, bit/hex literals, decimal or float
  values, parameters, variables, subqueries, or arbitrary expressions as
  arguments;
- fractional seconds;
- compact numeric temporal text, two-digit years, trailing-delimiter temporal
  truncation, ISO-like temporal strings, locale or time-zone coercion, or
  broader SQL-mode-sensitive temporal parsing;
- `TIME` descriptor arguments;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through;
- broader temporal functions such as `MAKEDATE()`, `TO_DAYS()`, or
  `TO_SECONDS()`.

## Grammar

MyLite adds these parser productions:

```lemon
expression(A) ::= DAYNAME(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= MONTHNAME(T) LPAREN expression(V) RPAREN(R).
```

Wrong arities produce argument-count AST nodes so runtime can return MySQL's
native-function parameter-count diagnostic:

```lemon
expression(A) ::= DAYNAME(T) LPAREN RPAREN(R).
expression(A) ::= DAYNAME(T) LPAREN expression(V) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= MONTHNAME(T) LPAREN RPAREN(R).
expression(A) ::= MONTHNAME(T) LPAREN expression(V) COMMA function_argument_list(C) RPAREN(R).
```

The function names remain usable as unquoted identifiers where MyLite admits
ordinary nonreserved keywords:

```lemon
identifier(A) ::= DAYNAME(T).
identifier(A) ::= MONTHNAME(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
calendar_name_expr(A) ::= calendar_name_name(T) LPAREN supported_calendar_name_value(V) RPAREN.

calendar_name_name ::= DAYNAME | MONTHNAME.

supported_calendar_name_value ::= NULL.
supported_calendar_name_value ::= string_literal.
supported_calendar_name_value ::= descriptor_date_column.
supported_calendar_name_value ::= descriptor_datetime_column.
supported_calendar_name_value ::= descriptor_timestamp_column.
supported_calendar_name_value ::= descriptor_string_column.
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
   physical column names. String literals and internal discriminators are bound
   parameters.
7. Use the existing MyLite-owned SQLite temporal scalar function for
   table-backed row execution.

Evaluation:

1. `NULL` returns `NULL`.
2. Canonical `YYYY-MM-DD` and `YYYY-MM-DD HH:MM:SS` strings are parsed by
   MyLite-owned temporal code.
3. `DAYNAME()` requires a complete valid calendar date. Valid year-zero dates
   are admitted except year zero is not leap.
4. `MONTHNAME()` requires `MM` in `1..12`; `DD = 00` is accepted when the year
   and month otherwise identify a valid month.
5. Invalid admitted string values return `NULL` and append warning `1292 /
   22007`, using the current MyLite warning stack. Invalid descriptor temporal
   values return `NULL` without a fresh warning, matching the existing
   calendar-date function policy.
6. Successful supported in-range values produce no warnings.

Output names:

- Weekday order is Sunday, Monday, Tuesday, Wednesday, Thursday, Friday,
  Saturday.
- Month order is January through December.
- Names are emitted as ASCII English text for the current slice.

## Metadata And Result Reporting

- No public ABI changes are required.
- Successful `SELECT` returns a row result set through the existing result API.
- Successful `DO` returns no result rows, `affected_rows = 0`, and
  `warning_count = 0` unless an evaluated expression warns.
- Successful scalar `SELECT` leaves session `ROW_COUNT()` at `-1`.
- Result-column labels use MyLite's existing expression text/alias rules.
- Result metadata is `VAR_STRING`, nullable, no flags, decimals `31`, and uses
  the current connection collation. With the default `utf8mb4_0900_ai_ci`
  connection collation the display length is `36`, matching MySQL's
  nine-character maximum calendar-name result multiplied by the connection
  character set's max bytes per character.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors outside the admitted grammar;
- wrong argument count for `DAYNAME()` and `MONTHNAME()`, returning
  `1582 / 42000`;
- unsupported argument expressions, returning MyLite's current unsupported
  parse diagnostic for calendar-name functions;
- unknown identifier arguments in row-backed selects, using the existing
  unknown-column diagnostic;
- `TIME` descriptor arguments, returning a deterministic unsupported diagnostic;
- string literals containing NUL bytes;
- invalid admitted string temporal values, returning `NULL` plus warning
  `1292 / 22007`;
- physical SQLite failures;
- allocation failures;
- public API misuse through existing public execution/result misuse behavior.

## Architecture And Ownership

- Public API: unchanged `mylite_execute()` and result APIs expose the behavior.
- Statement context: existing SQL mode and warning stack handling apply.
- Parser/AST: adds two one-argument function node kinds and two argument-count
  node kinds.
- Analyzer/planner: uses existing scalar and row-scalar expression admission and
  descriptor column resolution.
- Catalog: descriptors remain authoritative for table-backed column kind
  checks. No catalog rows, descriptor versions, descriptor caches, catalog
  generation, or SQLite schema generation values are mutated by these queries.
- Result builder: reuses scalar/row result creation and warning counts.
- Storage/VFS/file format: unchanged. The `.mylite` preamble and shifted SQLite
  payload invariants are unaffected.
- SQLite physical row storage: table-backed evaluation uses a public SQLite
  scalar function registered by MyLite. SQLite stores and scans rows; MyLite
  only supplies the compatibility function and argument binding. No SQLite fork
  patch is required.

## Performance

No-source and `DUAL` expressions are evaluated directly in MyLite once per
statement. Table-backed row-scalar projections stay in SQLite's scan path and
invoke a MyLite scalar callback per projected value. MyLite does not materialize
whole tables to compute these names.

## Test Plan

Tests must cover:

- no-source, `FROM DUAL`, and `DO` forms;
- row-backed projections over `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`,
  `VARCHAR`, and `TEXT` descriptor columns;
- persistence across close/reopen;
- supported valid dates, datetimes, pre-1000 dates, and year-zero dates;
- `DAYNAME()` full-zero, month-zero, day-zero, invalid day, invalid datetime
  time fields, non-temporal strings, and `NULL`;
- `MONTHNAME()` full-zero, month-zero, day-zero accepted where month is valid,
  invalid day, invalid datetime time fields, non-temporal strings, and `NULL`;
- warnings and `@@warning_count`;
- expression labels, aliases, whitespace before `(`, `ANSI_QUOTES`, and
  `NO_BACKSLASH_ESCAPES`;
- wrong argument counts, unknown columns, unsupported numeric/boolean/expression
  arguments, unsupported `TIME` descriptor columns, and NUL-containing string
  literals;
- parser coverage for function nodes, argument-count nodes, and unquoted
  identifier use;
- compatibility docs and existing parser/runtime lifecycle tests.
