# Baseline Week Temporal Functions

## Goal

Add a narrow week-oriented temporal function batch:

```sql
WEEK(expr)
WEEK(expr, mode)
WEEKDAY(expr)
WEEKOFYEAR(expr)
YEARWEEK(expr)
YEARWEEK(expr, mode)
```

This phase extends MyLite's existing no-source scalar, `DUAL`, `DO`, and
single-table row-scalar projection paths. It is not a general temporal
expression engine and does not add expression predicates, expression ordering,
DML assignment values, generated columns, defaults, casts, `DATE_FORMAT()` week
tokens, `default_week_format` system-variable mutability, or arbitrary nested
expression planning.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, `default_week_format` system variable:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- Existing MyLite temporal designs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-calendar-date-functions/specs.md`
  - `docs/specs/baseline-timestampdiff-function/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_week_temporal_functions_expectations.sh`.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `WEEKDAY(date)` returns `0..6` where Monday is `0` and Sunday is `6`.
- `WEEKOFYEAR(date)` is equivalent to `WEEK(date, 3)` and returns `1..53`.
- `WEEK(date)` uses the current session `default_week_format`. MySQL's default
  session value is `0`.
- `WEEK(date, mode)` and `YEARWEEK(date, mode)` use the integer mode argument.
  Numeric mode values outside `0..7` are accepted without warnings and use the
  low three bits of the integer value; for example `-1` behaves as mode `7`,
  `8` as mode `0`, and `9` as mode `1`.
- `WEEK(date, NULL)` and `YEARWEEK(date, NULL)` use mode `0`, not the current
  `default_week_format`.
- `YEARWEEK(date)` always uses mode `0`; it is not influenced by
  `default_week_format`.
- `WEEK()` with no arguments and `WEEK(date, mode, extra)` are syntax errors
  (`1064 / 42000`), while wrong arities for `WEEKDAY()`, `WEEKOFYEAR()`, and
  `YEARWEEK()` are native-function parameter-count errors (`1582 / 42000`).
- `NULL` date arguments make the result `NULL` without warnings.
- Week functions are strict for full-zero dates, month-zero dates, day-zero
  dates, invalid calendar days, and invalid datetime time fields. String inputs
  return `NULL` and append warning `1292 / 22007` with text beginning
  `Incorrect datetime value:`.
- Complete year-zero dates such as `'0000-01-02'` are accepted. Year zero is
  not treated as a leap year.
- Stored `DATE`, `DATETIME`, and `TIMESTAMP` descriptor values that are
  full-zero or partial-zero return `NULL` without adding a new warning; string
  descriptor values containing the same invalid text warn like string literals.
- MySQL accepts broader numeric temporal values, string mode values, boolean
  date coercion, trailing delimiters, `TIME` coercions, and some ISO-like
  strings with warning-producing truncation. This slice defers those coercions
  because they belong to broader temporal conversion and expression planning.
- Successful supported statements produce no warnings. A successful scalar
  `SELECT` makes a following `ROW_COUNT()` return `-1`; a successful `DO` makes
  it return `0`.

The observed mode table for `WEEK()` is:

| Mode | First Day | Range | Week 1 Rule |
| --- | --- | --- | --- |
| `0` | Sunday | `0..53` | first week with a Sunday in the calendar year |
| `1` | Monday | `0..53` | first week with four or more days in the calendar year |
| `2` | Sunday | `1..53` | first week with a Sunday in the calendar year |
| `3` | Monday | `1..53` | first week with four or more days in the calendar year |
| `4` | Sunday | `0..53` | first week with four or more days in the calendar year |
| `5` | Monday | `0..53` | first week with a Monday in the calendar year |
| `6` | Sunday | `1..53` | first week with four or more days in the calendar year |
| `7` | Monday | `1..53` | first week with a Monday in the calendar year |

For `0..53` modes, `WEEK()` returns `0` for dates before the first week of the
date's own calendar year. For `1..53` modes, `WEEK()` reports the adjacent
week-year's week number at year boundaries. `YEARWEEK()` always reports the
adjacent week-year as part of the integer result.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT week_temporal_item[, week_temporal_item ...]
SELECT week_temporal_item[, week_temporal_item ...] FROM DUAL
```

`DO` form:

```sql
DO week_temporal_expr[, week_temporal_expr ...]
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
week_temporal_expr:
    WEEK ( date_value )
  | WEEK ( date_value, mode_value )
  | WEEKDAY ( date_value )
  | WEEKOFYEAR ( date_value )
  | YEARWEEK ( date_value )
  | YEARWEEK ( date_value, mode_value )
```

`date_value` is one of:

- `NULL`;
- a single- or double-quoted string literal containing canonical `YYYY-MM-DD`
  or `YYYY-MM-DD HH:MM:SS`, including pre-1000 years and complete year-zero
  dates;
- a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
  family is `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline
  `TEXT`.

`mode_value` is one of:

- omitted;
- `NULL`, which uses mode `0`;
- a supported signed decimal integer literal;
- `TRUE` or `FALSE`, using `1` or `0`.

The resulting value is an integer or SQL `NULL`. The public result object uses
existing scalar and row-result conventions.

## Deferred Surface

This slice intentionally does not support:

- mutable or readable `default_week_format` system-variable behavior beyond
  treating MyLite's current fixed default as `0` for one-argument `WEEK()`;
- string, decimal, floating, hex, bit, user-variable, parameter, column, or
  expression mode values;
- numeric temporal literals, booleans as date values, bit/hex date literals,
  decimal or float date values, parameters, variables, subqueries, or arbitrary
  expressions as date arguments;
- fractional seconds;
- compact numeric temporal text, two-digit years, trailing-delimiter temporal
  truncation, ISO-like temporal strings, locale or time-zone coercion, or
  broader SQL-mode-sensitive temporal parsing;
- `TIME` descriptor arguments;
- use in `WHERE` beyond direct numeric extractor predicates and the narrow
  `numeric_temporal_extract +/- integer_literal` comparison form, `ORDER BY`,
  `GROUP BY`, `HAVING`, DML assignments, defaults, generated columns, indexes,
  constraints, joins, CTEs, or arbitrary SQLite pass-through;
- `DATE_FORMAT()` week format specifiers such as `%U`, `%u`, `%V`, `%v`, `%X`,
  and `%x`;
- broader temporal functions such as `TO_DAYS()` or `TO_SECONDS()`, and
  locale-sensitive behavior or broader coercions for the later
  `DAYNAME()` / `MONTHNAME()` slice.

## Grammar

MyLite adds these parser productions:

```lemon
expression(A) ::= WEEK(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= WEEK(T) LPAREN expression(V) COMMA expression(M) RPAREN(R).
expression(A) ::= WEEKDAY(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= WEEKOFYEAR(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= YEARWEEK(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= YEARWEEK(T) LPAREN expression(V) COMMA expression(M) RPAREN(R).
```

Wrong arities produce either MySQL-shaped syntax errors or argument-count AST
nodes matching observed MySQL behavior:

```lemon
expression(A) ::= WEEKDAY(T) LPAREN RPAREN(R).
expression(A) ::= WEEKDAY(T) LPAREN expression(V) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= WEEKOFYEAR(T) LPAREN RPAREN(R).
expression(A) ::= WEEKOFYEAR(T) LPAREN expression(V) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= YEARWEEK(T) LPAREN RPAREN(R).
expression(A) ::= YEARWEEK(T) LPAREN expression(V) COMMA expression(M) COMMA function_argument_list(C) RPAREN(R).
```

`WEEK()` with no arguments and `WEEK()` with more than two arguments remain
syntax errors in this slice.

The function names remain usable as unquoted identifiers where MyLite admits
ordinary nonreserved keywords:

```lemon
identifier(A) ::= WEEK(T).
identifier(A) ::= WEEKDAY(T).
identifier(A) ::= WEEKOFYEAR(T).
identifier(A) ::= YEARWEEK(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
week_temporal_expr(A) ::= week_temporal_name(T) LPAREN supported_date_value(V) RPAREN.
week_temporal_expr(A) ::= week_mode_name(T) LPAREN supported_date_value(V) COMMA supported_mode_value(M) RPAREN.

week_temporal_name ::= WEEK | WEEKDAY | WEEKOFYEAR | YEARWEEK.
week_mode_name ::= WEEK | YEARWEEK.

numeric_temporal_extract_predicate(A) ::=
    week_temporal_expr(B) comparison_operator predicate_integer_value(C).
numeric_temporal_extract_predicate(A) ::=
    week_temporal_expr(B) PLUS predicate_integer_value(C)
    comparison_operator predicate_integer_value(D).
numeric_temporal_extract_predicate(A) ::=
    week_temporal_expr(B) MINUS predicate_integer_value(C)
    comparison_operator predicate_integer_value(D).

supported_date_value ::= NULL.
supported_date_value ::= string_literal.
supported_date_value ::= descriptor_date_column.
supported_date_value ::= descriptor_datetime_column.
supported_date_value ::= descriptor_timestamp_column.
supported_date_value ::= descriptor_string_column.

supported_mode_value ::= NULL.
supported_mode_value ::= signed_decimal_integer_literal.
supported_mode_value ::= TRUE.
supported_mode_value ::= FALSE.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions, row-scalar projection attempts,
   and supported single-source numeric extractor predicates containing the
   admitted functions.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literals using the current statement SQL mode, including
   `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Reject unsupported date and mode argument kinds before generated SQLite SQL
   exists.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. String literals, internal discriminators, and mode
   values are bound parameters.
7. Use a MyLite-owned SQLite scalar function for table-backed row execution.
   The mode-aware internal shape is:

   ```sql
   _mylite_temporal_extract(value, function_kind, input_kind, mode)
   ```

   `function_kind` is one of `week`, `weekday`, `weekofyear`, or `yearweek`.
   `input_kind` is an internal discriminator such as `string`, `date`,
   `datetime`, or `timestamp`. `mode` is an internal integer value and is
   ignored for functions that do not take a mode. These values are not
   user-visible.

Evaluation:

1. `NULL` date values return `NULL`.
2. Date-only values use that date. Datetime and timestamp values use their date
   portion and ignore the time portion after validating that hour, minute, and
   second are in datetime range.
3. All four functions require a complete valid date. Full-zero, month-zero,
   day-zero, and invalid calendar days return `NULL`; string inputs append
   warning `1292 / 22007`.
4. Stored temporal descriptor full-zero or partial-zero values follow the
   observed MySQL descriptor behavior without adding fresh warnings; string
   descriptor values warn like string literals.
5. Mode values are normalized with `mode & 7` after converting the admitted
   literal to a signed integer. `NULL` mode values normalize to `0`.
6. One-argument `WEEK()` uses MyLite's current fixed default mode `0`.
   One-argument `YEARWEEK()` also uses mode `0`, matching MySQL.
7. Calendar calculations use the Gregorian shape observed in MySQL 8.4.9; year
   zero is valid for complete dates but is not a leap year.

`WEEKDAY()` maps MyLite's existing Sunday-first day-of-week calculation to
Monday `0` through Sunday `6`. `WEEKOFYEAR()` is computed exactly as
`WEEK(value, 3)`.

`WEEK()` computes the first week start for the date's calendar year using the
selected mode. For `0..53` modes, dates before that first week return `0` and
late year-boundary dates continue to count in the date's calendar year. For
`1..53` modes, dates before or after the current calendar year's week range use
the adjacent week-year's week number.

`YEARWEEK()` computes the same week starts, but always returns
`week_year * 100 + week_number`, so boundary dates can report a different year
from the date argument even when the corresponding `WEEK()` mode would return
`0`.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: unchanged. Diagnostics and row-count behavior remain
  statement-owned.
- Lexer/parser/AST: add tokens, AST node kinds, grammar, identifier fallback,
  and argument-count nodes where MySQL reports native parameter-count errors.
- Analyzer/planner: resolve descriptor arguments and reject unsupported shapes.
  It creates bound planned values and internal function calls; it does not ask
  SQLite metadata which columns exist.
- Catalog: read-only descriptor authority. These functions must not mutate
  descriptors, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: returns integer values or `NULL`, using aliases or source
  expression labels under existing conventions.
- Storage/VFS/file format: unchanged. These functions read existing SQLite
  physical row values and must not touch the `.mylite` preamble.
- SQLite integration: public SQLite scalar-function API only. No SQLite fork
  patch is needed for this slice.

## Diagnostics

Supported invalid temporal strings append MySQL-compatible warnings:

```text
Warning 1292 22007 Incorrect datetime value: '<value>'
```

Unsupported MyLite subset diagnostics are deterministic MyLite errors:

- unsupported date argument kinds: week temporal functions support only string
  temporal literals, `NULL`, and descriptor date/datetime/timestamp/string
  columns;
- unsupported mode argument kinds: `WEEK()` and `YEARWEEK()` mode support is
  limited to integer, boolean, and `NULL` literals;
- `TIME` descriptor date arguments: week temporal functions do not yet support
  `TIME` values;
- NUL bytes in string literals: week temporal function literals do not support
  NUL bytes;
- unknown descriptor columns use the existing unknown-column diagnostic;
- physical SQLite failures and allocation failures use existing MyLite runtime
  diagnostics.

Wrong arity diagnostics match observed MySQL behavior for the admitted parser
surface:

- `WEEK()` and `WEEK(date, mode, extra)` are syntax errors;
- wrong arities for `WEEKDAY()`, `WEEKOFYEAR()`, and `YEARWEEK()` return native
  function parameter-count errors.

## Compatibility And Performance

This slice keeps MyLite compatibility code in MyLite. SQLite receives ordinary
projection SQL plus calls to a small deterministic MyLite scalar function. The
row-backed path streams through SQLite rows and does not materialize result
sets for post-processing in MyLite. The only MyLite work per row is parsing the
single input value, applying the week calculation, and returning an integer.

No indexes, constraints, triggers, generated columns, catalog rows, physical
table layout, VFS behavior, or SQLite fork patches are added.
