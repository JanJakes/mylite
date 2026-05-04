# DATE() and DATEDIFF() scalar functions

## Scope

This feature implements the first MySQL 8.4.9-compatible date extraction and
date-difference scalar functions for expression contexts MyLite already
executes:

- `DATE(expr)`
- `DATEDIFF(expr1, expr2)`

The functions are supported in no-table `SELECT`, one-table `SELECT`
projection, `WHERE`, and `ORDER BY`, and supported single-table `UPDATE` and
`DELETE` expression paths.

Out of scope:

- `DATE_ADD()`, `DATE_SUB()`, `ADDDATE()`, and `SUBDATE()` interval arithmetic
  and interval grammar
- `DATE_FORMAT()`, `STR_TO_DATE()`, `UNIX_TIMESTAMP()`, week functions, named
  time zones, and locale-sensitive temporal names
- broad temporal type storage/conversion beyond parsing the scalar inputs used
  by this feature
- exact relaxed temporal delimiter deprecation warnings for every accepted
  nonstandard delimiter shape
- protocol binary temporal encoding beyond the current result metadata surface

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- MySQL 8.4 Reference Manual, Date and Time Literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, DATE, DATETIME, and TIMESTAMP Types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/current-temporal-functions/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/temporal-column-types/specs.md`

Runtime behavior was verified against the official `mysql:8.4.9` Docker image
in container `mylite-mysql-849`, using:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings
docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 Behavior

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

`DATE(expr)` extracts the date part from a date or datetime expression and
returns `NULL` when `expr` is `NULL`. The result value is displayed as
`YYYY-MM-DD`.

`DATEDIFF(expr1, expr2)` converts both operands to dates, ignores any time part,
and returns `expr1 - expr2` as a signed day count. The function returns `NULL`
when either operand is `NULL` or cannot be converted to a complete date.

Verified results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `DATE('2003-12-31 01:02:03')` | `2003-12-31` | none |
| `DATE('2024-02-29 23:59:59.123456')` | `2024-02-29` | none |
| `DATE(CURDATE())`, `DATE(NOW(6))`, `DATE(CURRENT_TIMESTAMP)` | statement date | none |
| `DATE(NULL)` | `NULL` | none |
| `DATEDIFF('2007-12-31 23:59:59','2007-12-30')` | `1` | none |
| `DATEDIFF('2010-11-30 23:59:59','2010-12-31')` | `-31` | none |
| `DATEDIFF('2024-03-01','2024-02-29')` | `1` | none |
| `DATEDIFF('2024-02-29 00:00:00','2024-02-29 23:59:59')` | `0` | none |
| `DATEDIFF(NULL,'2024-01-01')` | `NULL` | none |
| `DATEDIFF(NULL,'bad')` | `NULL` | one 1292 warning for `'bad'` |

Temporal string and numeric parsing follows MySQL's date/datetime literal
rules for this slice:

- dashed date strings with two- or four-digit years are accepted
- dashed datetime strings with seconds and optional fractional seconds are
  accepted
- compact strings of 6, 8, 12, or 14 digits are accepted when the resulting
  date and optional time are valid
- numeric values are interpreted as compact temporal values, with leading-zero
  padding to the nearest MySQL temporal width; for example `DATE(101)` returns
  `2000-01-01`
- two-digit years map `70` through `99` to `1970` through `1999`, and `00`
  through `69` to `2000` through `2069`
- year `0000` is accepted when month and day are nonzero and form a valid
  date; `0000-00-00`, zero month, zero day, invalid leap days, and impossible
  calendar dates return `NULL`

Verified edge cases:

| Expression | Result | Warnings |
| --- | --- | --- |
| `DATE('70-01-01')` | `1970-01-01` | none |
| `DATE('69-12-31')` | `2069-12-31` | none |
| `DATE('0000-01-01')` | `0000-01-01` | none |
| `DATEDIFF('0000-01-02','0000-01-01')` | `1` | none |
| `DATE('0000-02-29')` | `NULL` | 1292 |
| `DATE('bad')` | `NULL` | 1292 |
| `DATE('2024-02-30')` | `NULL` | 1292 |
| `DATE('2001-11-00')` | `NULL` | 1292 |
| `DATE('0000-00-00')` | `NULL` | 1292 |
| `DATE('2024-02-29foo')` | `2024-02-29` | 1292 truncation warning |
| `DATE('2024-02-29 99:99:99')` | `NULL` | 1292 |
| `DATEDIFF('bad','worse')` | `NULL` | two 1292 warnings |

Observed result metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| `DATE(...)` | `DATE` | 10 | 0 | binary (63) | `BINARY` | yes |
| `DATEDIFF(...)` | `LONGLONG` | 9 | 0 | binary (63) | `BINARY NUM` | yes |

Unsupported arities are native errors in MySQL. MyLite's first slice should
fail them deterministically through the current unsupported-call diagnostic
surface until exact native error codes for scalar arity are implemented.

## MyLite Compatibility Decisions

MyLite returns scalar temporal values through the existing expression value
model. `DATE()` therefore returns text in `YYYY-MM-DD` form while result
metadata advertises MySQL `DATE`. `DATEDIFF()` returns a signed integer value
with MySQL-compatible nullable `LONGLONG` metadata.

The first implementation accepts standard dashed date/datetime strings,
compact date/datetime strings, integer numeric inputs, approximate numeric
inputs by truncating the fractional part, and values produced by MyLite's
current temporal functions. It validates complete date parts and the supported
time part when one is present.

In strict `UPDATE` and `DELETE` expression paths, invalid temporal warnings
from `DATE()` and `DATEDIFF()` are promoted through MyLite's existing DML
warning policy so the statement fails and affected rows are rolled back.

Exact fidelity for deprecated relaxed delimiters, time-zone offsets in string
inputs, and every native warning message variant is deferred. The first slice
does preserve the important compatibility contract for applications:

- `NULL` propagation
- signed day differences over valid dates
- leap-day and month-boundary correctness
- zero month/day and invalid-date `NULL` results under the default strict mode
- 1292 warning counts for invalid non-`NULL` arguments
- `DATEDIFF()` evaluates both operands for warnings even when another operand
  is `NULL` or invalid

## MyLite Lemon Grammar Snippets

`DATE` is a nonreserved keyword in MyLite's lexer because it is also a column
type token. The parser must allow it in ordinary function-name position.
`DATEDIFF` already uses the generic identifier-style function-call path.

These snippets describe the intended MyLite grammar shape; they are not copied
from MySQL grammar.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.
function_name ::= DATE.

/* Runtime/binder validation limits these names to the supported arities. */
date_extract_function ::= DATE LPAREN expression RPAREN.
date_difference_function ::= DATEDIFF LPAREN expression COMMA expression RPAREN.
```

## Runtime Semantics

`DATE(expr)`:

1. Evaluate `expr`.
2. If the value is `NULL`, return `NULL` without warnings.
3. Convert the value to the supported date/datetime input surface.
4. If conversion succeeds, return `YYYY-MM-DD`.
5. If conversion fails, append warning 1292 and return `NULL`.

`DATEDIFF(expr1, expr2)`:

1. Evaluate `expr1`, then `expr2`.
2. Convert each non-`NULL` value independently, preserving warnings for both
   operands.
3. If either original operand is `NULL` or either conversion fails, return
   `NULL`.
4. Otherwise compute the proleptic day-number difference using MySQL-compatible
   leap-day behavior for supported dates, including non-leap year `0000`.

The functions do not change the `.mylite` file format, metadata catalog, or
SQLite storage layout.

## Tests

Parser tests:

- `SELECT DATE('2024-02-29'), DATEDIFF('2024-03-01','2024-02-29')`
- `DATE` token disambiguation between column type and function call
- nested calls such as `DATE(NOW())` and `DATEDIFF(CURDATE(), DATE(NOW()))`
- unsupported arity surfaces for `DATE()`, `DATE(a,b)`, `DATEDIFF(a)`, and
  `DATEDIFF(a,b,c)`

Runtime tests:

- no-table `SELECT` fixed literal extraction and day differences
- `NULL` propagation for both functions
- valid date, datetime, fractional datetime, current temporal, compact string,
  and numeric inputs
- invalid strings, zero date, zero month/day, impossible dates, and invalid
  time parts returning `NULL` with warning 1292
- `DATEDIFF()` warning counts when both operands are invalid and when one
  operand is `NULL`
- leap days, month boundaries, negative differences, and same-day differences
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignments/order/predicate paths and `DELETE` predicates
- strict DML warning promotion and rollback for invalid `DATE()` and
  `DATEDIFF()` inputs
- result metadata for `DATE()` and `DATEDIFF()`

## Compatibility Status

After implementation, `DATE()` and `DATEDIFF()` have partial compatibility
rather than complete compatibility because exact native arity diagnostics,
relaxed delimiter warning fidelity, time-zone-offset string parsing, and
broader temporal conversion/storage surfaces remain deferred.
