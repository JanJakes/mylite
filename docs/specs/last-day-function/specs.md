# LAST_DAY function

## Scope

This feature implements `LAST_DAY(expr)` for the scalar expression contexts
MyLite already executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- exact native error-code parity for unsupported arities
- time-zone-sensitive `TIMESTAMP` conversion
- full SQL-mode variants beyond MyLite's current strict-mode warning policy
- broad temporal literal forms not already handled by MyLite's date/datetime
  parser

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/date-and-datediff-functions/specs.md`
  - `docs/specs/temporal-part-functions/specs.md`
  - `docs/specs/scalar-built-in-functions/specs.md`

Runtime behavior was verified against the local MySQL 8.4.9 container with:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv
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

Verified results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `LAST_DAY('2024-02-10')` | `2024-02-29` | none |
| `LAST_DAY('2023-02-10 12:34:56')` | `2023-02-28` | none |
| `LAST_DAY('2024-04-30')` | `2024-04-30` | none |
| `LAST_DAY(NULL)` | `NULL` | none |
| `LAST_DAY('0000-01-00')` | `0000-01-31` | none |
| `LAST_DAY('2008-00-00')` | `NULL` | 1292 incorrect datetime |
| `LAST_DAY('0000-00-00 12:34:56')` | `NULL` | 1292 incorrect datetime |
| `LAST_DAY('2024-02-10foo')` | `2024-02-29` | 1292 truncated date |
| `LAST_DAY('2024-02-10 12:34:56foo')` | `2024-02-29` | 1292 truncated datetime |
| `LAST_DAY('bad')` | `NULL` | 1292 incorrect datetime |
| `LAST_DAY('2024-02-30')` | `NULL` | 1292 incorrect datetime |
| `LAST_DAY(240229)` | `2024-02-29` | none |
| `LAST_DAY(20240210123456)` | `2024-02-29` | none |
| `LAST_DAY(20240210.9)` | `2024-02-29` | 1292 truncated date |

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| `LAST_DAY(...)` | `DATE` | 10 | 0 | binary | `BINARY` | yes |

## MyLite Compatibility Decisions

`LAST_DAY()` should reuse MyLite's date/datetime parser with incomplete dates
allowed, then apply the MySQL-specific validity rule for this function:

- zero day is allowed when the month is valid, so `0000-01-00` returns
  `0000-01-31`
- zero month is invalid, so `2008-00-00` and `0000-00-00 12:34:56` return
  `NULL` with warning 1292

Valid input returns a DATE value whose day is the final day of the parsed
month. Time fields and fractional seconds do not appear in the result.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

`LAST_DAY()` uses the ordinary scalar function call shape.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression RPAREN.

function_name ::= identifier.
```

`LAST_DAY` remains an ordinary nonreserved function name and may be used as an
identifier when it is not called.

## Runtime Semantics

1. Evaluate the argument expression.
2. Return `NULL` without warnings when the argument is `NULL`.
3. Convert the value through MyLite's date/datetime parser with incomplete dates
   accepted.
4. Return `NULL` and append warning 1292 when parsing fails.
5. Return `NULL` and append warning 1292 when the parsed month is zero.
6. Replace the parsed day with the month-end day and return a DATE result.

## Tests

Parser tests should cover:

- ordinary one-argument function-call parsing
- use of `last_day` as an identifier when not called

Runtime tests should cover:

- metadata for non-`NULL` and `NULL` expressions
- leap and non-leap month ends
- month-end input that is already the final day
- `NULL` propagation
- zero-day valid-month input
- zero-month and all-zero invalid inputs
- truncated date and datetime warnings
- malformed and impossible dates
- compact integer and decimal numeric inputs
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment, `DELETE` predicate, and strict DML warning promotion
- rejected unsupported arities

## Compatibility Status

After implementation, `LAST_DAY()` has partial compatibility for supported
scalar expression paths and MyLite's current date/datetime input surface.
Exact native diagnostics, full SQL-mode variants, broader temporal conversion,
and time-zone-sensitive `TIMESTAMP` behavior remain deferred.
