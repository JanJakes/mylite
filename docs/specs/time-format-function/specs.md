# TIME_FORMAT function

## Scope

This feature implements `TIME_FORMAT(time, format)` for the scalar expression
contexts MyLite currently executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- exact native error-code parity for unsupported arities
- locale-sensitive names, because name-producing date tokens return `NULL` for
  `TIME_FORMAT()`
- broader temporal literal variants not already handled by MyLite's time parser

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/date-format-function/specs.md`
  - `docs/specs/time-function/specs.md`
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
| `TIME_FORMAT('100:00:00', '%H %k %h %I %l')` | `100 100 04 04 4` | none |
| `TIME_FORMAT('12:34:56.123456', '%H:%i:%s.%f %r %T %p')` | `12:34:56.123456 12:34:56 PM 12:34:56 PM` | none |
| `TIME_FORMAT('-12:34:56', '%H %k %h %I %l %p %r %T')` | `-12 12 12 12 12 PM 12:34:56 PM 12:34:56` | none |
| `TIME_FORMAT('12:34:56', '%Y %y %m %c %d %e')` | `0000 00 00 0 00 0` | none |
| `TIME_FORMAT('12:34:56', '%W')` | `NULL` | none |
| `TIME_FORMAT(NULL, '%H')` | `NULL` | none |
| `TIME_FORMAT('12:34:56', NULL)` | `NULL` | none |
| `TIME_FORMAT('bad', '%H')` | `NULL` | 1292 truncated time |
| `TIME_FORMAT('839:00:00', '%H')` | `838` | 1292 truncated time |
| `TIME_FORMAT('2024-02-29 12:34:56', '%H:%i:%s')` | `12:34:56` | none |

Observed metadata is a nullable `VAR_STRING` in the connection character set and
collation, with `decimals = 31`. MyLite infers literal-format token expansion
and estimates dynamic format width from the format expression descriptor.

## MyLite Compatibility Decisions

`TIME_FORMAT()` reuses MyLite's `TIME()` coercion path for the first argument
and string coercion for the format argument. Invalid time values return `NULL`
while preserving the time-conversion warning. If either argument is `NULL`, the
result is `NULL` without additional warnings.

The first slice supports these token classes:

- time tokens: `%f`, `%H`, `%h`, `%I`, `%i`, `%k`, `%l`, `%p`, `%r`, `%S`,
  `%s`, and `%T`
- zero-valued date tokens accepted by MySQL for time-only input: `%Y`, `%y`,
  `%m`, `%c`, `%d`, and `%e`
- escaped and unknown tokens: `%%`, trailing `%`, and `%x` for unrecognized
  token characters
- `NULL` result tokens: weekday, month-name, day-ordinal, day-of-year, week,
  and week-year tokens

For negative time values, `%H` includes the sign and all other time tokens use
the absolute time components. The 12-hour clock and meridiem tokens use the
absolute hour modulo 24, while `%H`, `%k`, and `%T` preserve over-24-hour time
values, matching observed MySQL behavior.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

`TIME_FORMAT()` uses the ordinary scalar function call shape.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression COMMA expression RPAREN.

function_name ::= identifier.
```

`TIME_FORMAT` remains an ordinary nonreserved function name and may be used as
an identifier when it is not called.

## Runtime Semantics

1. Evaluate the time expression.
2. Evaluate the format expression.
3. Return `NULL` without warnings if either argument is `NULL`.
4. Convert the first argument through the same coercion path as `TIME()`.
5. Return `NULL` when time conversion fails, preserving warnings.
6. Walk the format string and append supported token output.
7. Return `NULL` if a format token maps to MySQL's time-format `NULL` token
   class.

## Tests

Parser tests should cover:

- ordinary two-argument function-call parsing
- use of `time_format` as an identifier when not called

Runtime tests should cover:

- result metadata for constant and `NULL` expressions
- ordinary, fractional, over-24-hour, and negative time formatting
- zero-valued date tokens
- `NULL` result tokens
- invalid, empty, clipped, datetime, compact, and numeric time inputs
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment, `DELETE` predicate, and strict DML warning promotion
- rejected unsupported arities

## Compatibility Status

After implementation, `TIME_FORMAT()` has partial compatibility for supported
scalar expression paths and MyLite's current time input surface. Exact native
diagnostics, broader temporal literal variants, and protocol metadata remain
deferred.
