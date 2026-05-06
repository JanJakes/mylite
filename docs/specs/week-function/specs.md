# `WEEK()`

## Scope

This feature implements `WEEK(date)` and `WEEK(date, mode)` for the scalar
expression paths MyLite already supports:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` and `DELETE` expression paths

Out of scope:

- mutable `default_week_format`; one-argument `WEEK(date)` uses MySQL's default
  mode `0` until that system variable is implemented
- `WEEKOFYEAR()`, `YEARWEEK()`, and `WEEKDAY()`
- `EXTRACT(WEEK FROM ...)`
- exact native arity diagnostics for unsupported call shapes

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- MySQL 8.4 Reference Manual, Server System Variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/temporal-part-functions/specs.md`
  - `docs/specs/date-format-function/specs.md`

Runtime behavior was verified against the local MySQL 8.4.9 container with:

```sh
docker exec mylite-mysql-849 mysql -uroot --batch --raw --skip-column-names
docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 Behavior

`WEEK(date[, mode])` returns the week number for a complete date. The optional
`mode` selects the first day of week, whether week numbers can be `0`, and how
week 1 is chosen.

Verified mode table:

| Mode | First day | Range | Week 1 rule |
| ---: | --- | --- | --- |
| 0 | Sunday | 0-53 | first week with a Sunday in this year |
| 1 | Monday | 0-53 | first week with at least four days in this year |
| 2 | Sunday | 1-53 | first week with a Sunday in this year |
| 3 | Monday | 1-53 | first week with at least four days in this year |
| 4 | Sunday | 0-53 | first week with at least four days in this year |
| 5 | Monday | 0-53 | first week with a Monday in this year |
| 6 | Sunday | 1-53 | first week with at least four days in this year |
| 7 | Monday | 1-53 | first week with a Monday in this year |

Observed no-table results:

| Expression | Result | Warnings |
| --- | ---: | --- |
| `@@default_week_format` | `0` | none |
| `WEEK('2008-02-20')` | `7` | none |
| `WEEK('2008-02-20',0)` | `7` | none |
| `WEEK('2008-02-20',1)` | `8` | none |
| `WEEK('2008-12-31',1)` | `53` | none |
| `WEEK('2000-01-01',0)` | `0` | none |
| `WEEK('2000-01-01',2)` | `52` | none |
| `WEEK('2024-01-01',0)` | `0` | none |
| `WEEK('2024-01-01',1)` | `1` | none |
| `WEEK('2024-01-01',2)` | `53` | none |
| `WEEK('2024-01-01',3)` | `1` | none |
| `WEEK('2024-01-01',4)` | `1` | none |
| `WEEK('2024-01-01',5)` | `1` | none |
| `WEEK('2024-01-01',6)` | `1` | none |
| `WEEK('2024-01-01',7)` | `1` | none |
| `WEEK('2018-12-31',1)` | `53` | none |
| `WEEK('2018-12-31',3)` | `1` | none |
| `WEEK(NULL)` | `NULL` | none |
| `WEEK('2024-01-01', NULL)` | `0` | none |
| `WEEK(NULL, 3)` | `NULL` | none |
| `WEEK('bad')` | `NULL` | 1292 |
| `WEEK('2008-00-00')` | `NULL` | 1292 |
| `WEEK('2008-01-00')` | `NULL` | 1292 |

`mode` conversion has MySQL-specific details:

| Expression | Result | Warnings |
| --- | ---: | --- |
| `WEEK('2024-01-01','3')` | `1` | none |
| `WEEK('2024-01-01','bad')` | `0` | 1292 truncated integer |
| `WEEK('2000-01-01',8)` | `0` | none |
| `WEEK('2000-01-01',9)` | `0` | none |
| `WEEK('2000-01-01',-1)` | `52` | none |
| `WEEK('2000-01-01',15)` | `52` | none |
| `WEEK('2000-01-01',16)` | `0` | none |
| `WEEK('2000-01-01',1.9)` | `52` | none |
| `WEEK('2000-01-01','1.9')` | `0` | 1292 truncated integer |

The runtime behavior above is equivalent to converting `mode` to an integer and
using only its low three bits. Approximate numeric modes round to integer;
string modes parse an integer prefix and warn on trailing garbage.

Observed metadata:

| Expression | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| `WEEK(...)` | `LONGLONG` | 3 | 0 | binary | `BINARY NUM` | yes |

## MyLite Compatibility Decisions

MyLite should reuse the existing MyLite-owned date/datetime parser used by
`DAYOFWEEK()` and `DAYOFYEAR()`. `WEEK()` requires a complete valid date:
incomplete dates such as `2008-00-00` or `2008-01-00` return `NULL` with
warning 1292. Date and datetime inputs ignore the time portion.

One-argument `WEEK(date)` uses mode `0`. This matches a default MySQL 8.4.9
session where `default_week_format` is `0`; changing that variable remains
deferred and documented separately in `COMPATIBILITY.md`.

The `mode` argument is evaluated only after a non-`NULL` date has been parsed as
a complete date. A `NULL` mode behaves as `0`. Non-`NULL` mode values are
converted through MyLite's existing signed-integer conversion so string
truncation warnings and approximate numeric rounding match the current scalar
conversion rules.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

These snippets describe the intended MyLite grammar shape; they are not copied
from MySQL grammar.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression RPAREN.
scalar_function_call ::= function_name LPAREN expression COMMA expression RPAREN.

function_name ::= identifier.
```

`WEEK` remains an ordinary identifier-style function name outside existing
interval-unit positions.

## Runtime Semantics

For `WEEK(date[, mode])`:

1. Evaluate `date`.
2. If `date` is `NULL`, evaluate the optional `mode` expression for expression
   warnings only, then return `NULL`.
3. Convert `date` through the complete date/datetime parser.
4. Return `NULL` and append warning 1292 for malformed, impossible, zero, or
   incomplete non-`NULL` dates.
5. Evaluate and convert `mode` when supplied. Use mode `0` when omitted or
   `NULL`; otherwise mask the converted integer to the low three bits.
6. Compute the week number for the normalized mode.

For modes that allow week `0`, dates before week 1 of their calendar year return
`0`. For modes with range `1-53`, dates before week 1 use the final week number
of the previous year, and dates at or beyond week 1 of the next year return `1`.

## Tests

Parser tests should cover:

- `WEEK(expr)` and `WEEK(expr, expr)` function-call parsing
- lower-case `week` as an identifier when not called
- rejected empty and three-argument arities at prepare/runtime validation

Runtime tests should cover:

- default one-argument behavior
- all eight mode values
- first-week and last-week boundary dates
- `NULL` date and `NULL` mode handling
- mode masking for out-of-range and negative values
- mode conversion warnings for invalid string modes
- invalid, zero, and incomplete date warnings
- result metadata
- table projection, `WHERE`, `ORDER BY`, `UPDATE`, and `DELETE`
- strict DML warning promotion and rollback for invalid temporal inputs

## Compatibility Status

After implementation, `WEEK()` has partial compatibility for the supported
scalar expression paths. Mutable `default_week_format`, `WEEKOFYEAR()`,
`YEARWEEK()`, `WEEKDAY()`, `EXTRACT(WEEK FROM ...)`, exact native arity
diagnostics, and broader SQL-mode temporal variants remain deferred.
