# SEC_TO_TIME function

## Scope

This feature implements `SEC_TO_TIME(expr)` for the scalar expression contexts
MyLite currently executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- exact native error-code parity for unsupported arities
- exact range-warning expression rendering for every approximate or truncated
  numeric input
- full fixed-point DECIMAL storage fidelity for every runtime expression value
- broader SQL-mode variants beyond MyLite's current strict-mode warning policy

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/time-function/specs.md`
  - `docs/specs/time-to-sec-function/specs.md`

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
| `SEC_TO_TIME(2378)` | `00:39:38` | none |
| `SEC_TO_TIME(0)` | `00:00:00` | none |
| `SEC_TO_TIME(-45296)` | `-12:34:56` | none |
| `SEC_TO_TIME(3020399)` | `838:59:59` | none |
| `SEC_TO_TIME(-3020399)` | `-838:59:59` | none |
| `SEC_TO_TIME(NULL)` | `NULL` | none |
| `SEC_TO_TIME(3020400)` | `838:59:59` | 1292 truncated time |
| `SEC_TO_TIME(-3020400)` | `-838:59:59` | 1292 truncated time |
| `SEC_TO_TIME(1.50)` | `00:00:01.50` | none |
| `SEC_TO_TIME(1.9999995)` | `00:00:02.000000` | none |
| `SEC_TO_TIME(-1.9999995)` | `-00:00:02.000000` | none |
| `SEC_TO_TIME('2378')` | `00:39:38.000000` | none |
| `SEC_TO_TIME('2378x')` | `00:39:38.000000` | 1292 truncated DECIMAL |
| `SEC_TO_TIME('x2378')` | `00:00:00.000000` | 1292 truncated DECIMAL |
| `SEC_TO_TIME('1e2')` | `00:01:40.000000` | none |

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| integer or `NULL` seconds | `TIME` | 10 | 0 | binary | `BINARY` | yes |
| exact decimal seconds | `TIME` | `11 + scale` | scale capped at 6 | binary | `BINARY` | yes |
| approximate or string seconds | `TIME` | 17 | 6 | binary | `BINARY` | yes |

## MyLite Compatibility Decisions

`SEC_TO_TIME()` converts its argument through MySQL-style DECIMAL numeric
coercion, not through the temporal time parser. Numeric text without a valid
prefix becomes zero and emits warning 1292. Numeric text with trailing junk uses
the valid numeric prefix and emits the same DECIMAL truncation warning.

The converted seconds are rounded to microseconds, split into signed
`HH:MM:SS[.fraction]`, and clipped to the MySQL `TIME` range
`-838:59:59` through `838:59:59`. Range clipping emits warning 1292 with the
time-value wording. Fractional display precision is based on the argument:

- integer and `NULL` arguments use precision 0
- exact DECIMAL expressions use the argument scale, capped at 6
- approximate and textual expressions use precision 6

The first runtime slice preserves exact scale for numeric literals and common
numeric values. Full fixed-point DECIMAL column and expression value fidelity
remains deferred with the broader DECIMAL runtime-storage work.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

`SEC_TO_TIME()` uses the ordinary scalar function call shape.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression RPAREN.

function_name ::= identifier.
```

`SEC_TO_TIME` remains an ordinary nonreserved function name and may be used as
an identifier when it is not called.

## Runtime Semantics

1. Evaluate the argument expression.
2. Return `NULL` without warnings when the argument is `NULL`.
3. Convert the value to DECIMAL-style numeric seconds, preserving conversion
   warnings.
4. Round the absolute value to whole microseconds.
5. Clip out-of-range magnitudes to MySQL's maximum `TIME` value and emit a
   time-value truncation warning.
6. Format the signed `TIME` result with the selected fractional precision.

## Tests

Parser tests should cover:

- ordinary one-argument function-call parsing
- use of `sec_to_time` as an identifier when not called

Runtime tests should cover:

- result metadata for integer, decimal, string, and `NULL` expressions
- positive, zero, negative, min, max, and `NULL` inputs
- range clipping on both signs with warning 1292
- fractional rounding into the next second
- string numeric prefixes, invalid strings, empty strings, and exponent strings
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment, `DELETE` predicate, and strict DML warning promotion
- rejected unsupported arities

## Compatibility Status

After implementation, `SEC_TO_TIME()` has partial compatibility for supported
scalar expression paths and MyLite's current numeric conversion surface. Exact
native diagnostics, exact range-warning rendering for every expression shape,
full fixed-point DECIMAL runtime storage, and protocol metadata remain
deferred.
