# Baseline Calendar Date Functions

## Goal

Add a narrow calendar-date function batch:

```sql
DAYOFWEEK(expr)
DAYOFYEAR(expr)
LAST_DAY(expr)
```

This phase extends MyLite's existing no-source scalar, `DUAL`, `DO`, and
single-table row-scalar projection paths. It is not a general temporal
expression engine and does not add expression predicates, expression ordering,
DML assignment values, generated columns, defaults, casts, or arbitrary nested
expression planning.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing MyLite temporal designs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-datediff-function/specs.md`
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_calendar_date_functions_expectations.sh`.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `DAYOFWEEK(date)` returns `1..7` where Sunday is `1`.
- `DAYOFYEAR(date)` returns `1..366`.
- `LAST_DAY(date)` returns the last day of the argument's month as
  `YYYY-MM-DD`.
- Datetime arguments use their date portion when the time fields are valid
  `00:00:00` through `23:59:59`.
- `NULL` arguments make the result `NULL` without warnings.
- Pre-1000 canonical dates such as `'0001-01-01'` and `'0999-12-31'`
  are accepted for these functions without warnings.
- Complete year-zero dates such as `'0000-01-02'` are accepted. Year zero is
  not treated as a leap year for these functions.
- `DAYOFWEEK()` and `DAYOFYEAR()` reject full-zero and partial-zero dates,
  return `NULL`, and append warning `1292 / 22007` with text beginning
  `Incorrect datetime value:`.
- `LAST_DAY()` rejects full-zero dates, month-zero dates, and invalid nonzero
  days, but accepts a zero day when the year and month are otherwise valid:
  `LAST_DAY('2001-11-00')` returns `2001-11-30` without a warning.
- Datetime strings with invalid time fields such as `24:00:00` or `99:00:00`
  return `NULL` and append warning `1292 / 22007`.
- Stored `DATE`, `DATETIME`, and `TIMESTAMP` descriptor values that are
  full-zero or partial-zero return `NULL` or the accepted `LAST_DAY()` value
  without adding a new warning; string descriptor values containing the same
  invalid text warn like string literals.
- Non-temporal strings return `NULL` and warn with `1292 / 22007`.
- MySQL accepts broader numeric temporal values, boolean coercion, trailing
  delimiters, and some ISO-like strings with warning-producing truncation. This
  slice defers those coercions because they belong to the broader temporal
  conversion model.
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
SELECT calendar_date_item[, calendar_date_item ...]
SELECT calendar_date_item[, calendar_date_item ...] FROM DUAL
```

`DO` form:

```sql
DO calendar_date_expr[, calendar_date_expr ...]
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
calendar_date_expr:
    DAYOFWEEK ( date_value )
  | DAYOFYEAR ( date_value )
  | LAST_DAY ( date_value )
```

`date_value` is one of:

- `NULL`;
- a single- or double-quoted string literal containing canonical `YYYY-MM-DD`
  or `YYYY-MM-DD HH:MM:SS`, including pre-1000 years;
- the same canonical shape with complete year-zero dates;
- for `LAST_DAY()` only, the same shape with `DD = 00` when `YYYY` and `MM`
  otherwise identify a valid month;
- a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
  family is `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline
  `TEXT`.

The resulting value is an integer for `DAYOFWEEK()` and `DAYOFYEAR()`, date
text for `LAST_DAY()`, or SQL `NULL`. The public result object uses existing
scalar and row-result conventions.

## Deferred Surface

This slice intentionally does not support:

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
- broader locale-aware `DAYNAME()` / `MONTHNAME()` behavior and temporal
  coercions outside the later baseline calendar-name slice;
- broader temporal functions such as `EXTRACT()`, `TIMESTAMPDIFF()`,
  `TO_DAYS()`, `TO_SECONDS()`, `WEEK()`, `WEEKDAY()`, `WEEKOFYEAR()`, or
  `YEARWEEK()`.

## Grammar

MyLite adds these parser productions:

```lemon
expression(A) ::= DAYOFWEEK(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= DAYOFYEAR(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= LAST_DAY(T) LPAREN expression(V) RPAREN(R).
```

Wrong arities produce argument-count AST nodes so runtime can return MySQL's
native-function parameter-count diagnostic:

```lemon
expression(A) ::= DAYOFWEEK(T) LPAREN RPAREN(R).
expression(A) ::= DAYOFWEEK(T) LPAREN expression(V) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= DAYOFYEAR(T) LPAREN RPAREN(R).
expression(A) ::= DAYOFYEAR(T) LPAREN expression(V) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= LAST_DAY(T) LPAREN RPAREN(R).
expression(A) ::= LAST_DAY(T) LPAREN expression(V) COMMA function_argument_list(C) RPAREN(R).
```

The function names remain usable as unquoted identifiers where MyLite admits
ordinary nonreserved keywords:

```lemon
identifier(A) ::= DAYOFWEEK(T).
identifier(A) ::= DAYOFYEAR(T).
identifier(A) ::= LAST_DAY(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
calendar_date_expr(A) ::= calendar_date_name(T) LPAREN supported_calendar_date_value(V) RPAREN.

calendar_date_name ::= DAYOFWEEK | DAYOFYEAR | LAST_DAY.

supported_calendar_date_value ::= NULL.
supported_calendar_date_value ::= string_literal.
supported_calendar_date_value ::= descriptor_date_column.
supported_calendar_date_value ::= descriptor_datetime_column.
supported_calendar_date_value ::= descriptor_timestamp_column.
supported_calendar_date_value ::= descriptor_string_column.
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
2. Date-only values use that date. Datetime and timestamp values use their date
   portion and ignore the time portion after validating that hour, minute, and
   second are in datetime range.
3. `DAYOFWEEK()` and `DAYOFYEAR()` require a complete valid date. Full-zero,
   month-zero, day-zero, and invalid calendar days return `NULL`; string inputs
   append warning `1292 / 22007`.
4. `LAST_DAY()` requires a valid nonzero month. A zero day is accepted for
   otherwise valid year/month inputs and is ignored when calculating month end.
   Invalid string inputs append warning `1292 / 22007`.
5. Stored temporal descriptor full-zero or partial-zero values follow the
   observed MySQL descriptor behavior without adding fresh warnings; string
   descriptor values warn like string literals.
6. Calendar calculations use the Gregorian shape observed in MySQL 8.4.9; year
   zero is valid for complete dates but is not a leap year.

The row-backed generated SQL shape remains:

```sql
_mylite_temporal_extract(value, function_kind, input_kind)
```

`function_kind` is one of `dayofweek`, `dayofyear`, or `last_day` for this
slice. `input_kind` is an internal discriminator such as `string`, `date`,
`datetime`, or `timestamp`. These values are not user-visible.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: unchanged. Diagnostics and row-count behavior remain
  statement-owned.
- Lexer/parser/AST: add tokens, AST node kinds, grammar, identifier fallback,
  and argument-count nodes.
- Analyzer/planner: resolve descriptor arguments and reject unsupported shapes.
  It creates bound planned values and internal function calls; it does not ask
  SQLite metadata which columns exist.
- Catalog: read-only descriptor authority. These functions must not mutate
  descriptors, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: returns integer/date text or `NULL`, using aliases or source
  spans for labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use the existing public scalar-function registration path for
  `_mylite_temporal_extract`. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- wrong arity: `1582 / 42000`, `Incorrect parameter count in the call to
  native function '<name>'`;
- invalid non-`NULL` temporal string input: warning `1292 / 22007`,
  `Incorrect datetime value: 'value'`;
- unknown descriptor column: existing MySQL-compatible unknown-column
  diagnostic;
- unsupported argument literal/expression:
  `calendar date functions support only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor columns, string descriptor columns, and NULL`;
- unsupported `TIME` descriptor column:
  `calendar date functions do not yet support TIME values`;
- unsupported NUL-containing literal:
  `calendar date function literals do not support NUL bytes`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`: mark `DAYOFWEEK()`, `DAYOFYEAR()`, and `LAST_DAY()` as
  limited.
- `docs/compatibility/functions-temporal.md`: document the exact scalar and
  row-scalar projection subset.
- `docs/compatibility/sql-query-expressions.md` and
  `docs/compatibility/type-system-literals-conversion.md`: mention this
  admitted scalar/row-scalar function context only where needed.

Do not claim support for locale names, week functions, `EXTRACT()`, full
temporal coercion, temporal predicates, expression ordering, or general
expression evaluation.

## Tests

Add fast C tests under `packages/libmylite/tests/` and register them as
`libmylite.runtime.calendar_date_functions`.

Coverage:

- no-source and `DUAL` scalar values for valid date/datetime strings;
- `NULL`, full-zero, partial-zero, year-zero, leap-year, and invalid-date
  behavior;
- `LAST_DAY()` zero-day acceptance and invalid month/day rejection;
- `DO` status, affected rows, and warnings;
- labels, whitespace before `(`, identifier fallback, `ANSI_QUOTES`, and
  `NO_BACKSLASH_ESCAPES`;
- table-backed row-scalar projection over `DATE`, `DATETIME`, `TIMESTAMP`, and
  string descriptor columns;
- existing `WHERE`, `ORDER BY`, and `LIMIT` row envelope preservation;
- reopen persistence where the existing row envelope is involved;
- unknown columns, `TIME` descriptors, unsupported literals/expressions, wrong
  arities, and NUL-containing string literals;
- focused parser coverage;
- no catalog generation, SQLite schema generation, or file-format mutation.

Run:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'libmylite\.(parser|runtime\.(calendar_date_functions|temporal_extract_functions|datediff_function|date_format_function))'
sh packages/libmylite/tests/mysql_baseline_calendar_date_functions_expectations.sh
cmake --workflow --preset check
```
