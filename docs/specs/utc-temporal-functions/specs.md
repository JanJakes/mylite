# UTC current temporal functions

## Scope

This feature implements the MySQL current UTC temporal functions for scalar
expression contexts MyLite already executes:

- `UTC_DATE`, `UTC_DATE()`
- `UTC_TIME`, `UTC_TIME([fsp])`
- `UTC_TIMESTAMP`, `UTC_TIMESTAMP([fsp])`

The functions are supported in no-table `SELECT`, one-table `SELECT`
projection, `WHERE`, and `ORDER BY`, and supported single-table `UPDATE` and
`DELETE` expression paths.

Out of scope:

- `SET timestamp` clock overrides
- session time-zone variables and named time-zone tables
- exact native error-code parity for invalid fractional precision
- protocol-level binary temporal value encoding beyond existing metadata
  descriptors

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/current-temporal-functions/specs.md`
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

MySQL evaluates current temporal functions once at statement start. The UTC
variants use the same statement timestamp but format it in UTC instead of the
session time zone.

With `SET time_zone = '+02:30'` and `SET timestamp = 1700000000.987654`, the
verified runtime produced:

| Expression | Result |
| --- | --- |
| `UTC_DATE` | `2023-11-14` |
| `UTC_DATE()` | `2023-11-14` |
| `UTC_DATE() + 0` | `20231114` |
| `UTC_TIME` | `22:13:20` |
| `UTC_TIME()` | `22:13:20` |
| `UTC_TIME(3)` | `22:13:20.987` |
| `UTC_TIME(6)` | `22:13:20.987654` |
| `UTC_TIME() + 0` | `221320` |
| `UTC_TIMESTAMP` | `2023-11-14 22:13:20` |
| `UTC_TIMESTAMP()` | `2023-11-14 22:13:20` |
| `UTC_TIMESTAMP(3)` | `2023-11-14 22:13:20.987` |
| `UTC_TIMESTAMP(6)` | `2023-11-14 22:13:20.987654` |
| `UTC_TIMESTAMP() + 0` | `20231114221320` |

Invalid forms:

| Expression | MySQL behavior |
| --- | --- |
| `UTC_DATE(0)` | syntax error 1064 |
| `UTC_TIME(7)` | error 1426, precision greater than 6 |
| `UTC_TIMESTAMP(7)` | error 1426, precision greater than 6 |
| `UTC_TIME('3')` | syntax error 1064 |
| `UTC_TIMESTAMP('3')` | syntax error 1064 |
| `UTC_TIMESTAMP(1,2)` | syntax error 1064 |

Observed result metadata:

| Expression family | Field type | Length at `fsp=0` | Length at `fsp=n>0` | Decimals | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `UTC_DATE` | `DATE` | 10 | 10 | 0 | `NOT_NULL BINARY` |
| `UTC_TIME` | `TIME` | 8 | `9 + n` | `n` | `NOT_NULL BINARY` |
| `UTC_TIMESTAMP` | `DATETIME` | 19 | `20 + n` | `n` | `NOT_NULL BINARY` |

## MyLite Compatibility Decisions

MyLite already captures one UTC statement timestamp for the existing current
temporal functions. Because MyLite does not yet expose mutable session time-zone
state, the local/session functions and UTC functions currently share the same
formatted value. This is intentional for the first slice and keeps host-local
time-zone settings from affecting embedded results.

`UTC_DATE` uses the existing current-date descriptor and formatting path.
`UTC_TIME` uses the existing current-time descriptor and formatting path.
`UTC_TIMESTAMP` uses the existing current-datetime descriptor and formatting
path. Fractional precision validation rejects invalid calls at prepare time
through MyLite's generic unsupported-statement diagnostic until exact native
scalar-function error mapping is implemented.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

The UTC functions use the same shapes as the existing current temporal
functions:

```lemon
primary_expression ::= bare_current_temporal_function.
primary_expression ::= scalar_function_call.

bare_current_temporal_function ::= UTC_DATE.
bare_current_temporal_function ::= UTC_TIME.
bare_current_temporal_function ::= UTC_TIMESTAMP.

scalar_function_call ::= function_name LPAREN RPAREN.
scalar_function_call ::= function_name LPAREN expression RPAREN.

function_name ::= UTC_DATE.
function_name ::= UTC_TIME.
function_name ::= UTC_TIMESTAMP.
```

`UTC_DATE` accepts no arguments. `UTC_TIME` and `UTC_TIMESTAMP` accept no
arguments or a single integer fractional-seconds precision from 0 through 6.

## Runtime Semantics

1. Resolve the function name through the current-temporal runtime.
2. Capture the statement timestamp lazily if it has not already been captured.
3. Reuse that timestamp for every later current temporal function in the same
   statement, including per-row evaluation.
4. Format the value as UTC date, time, or datetime text.
5. Set the value's temporal type so downstream conversion treats it as `DATE`,
   `TIME`, or `DATETIME`.

## Tests

Parser coverage already accepts bare and parenthesized UTC spellings. Runtime
tests should cover:

- bare and parenthesized scalar forms
- fractional precision for `UTC_TIME` and `UTC_TIMESTAMP`
- result metadata for `DATE`, `TIME`, and `DATETIME`
- derivation of `UTC_DATE` and `UTC_TIME` from the same `UTC_TIMESTAMP`
- one-table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignments and `DELETE` predicates
- invalid precision and invalid arity diagnostics under MyLite's current
  diagnostic policy

## Compatibility Status

After implementation, the UTC current temporal functions are partially
compatible for supported expression paths. Exact native diagnostics,
`SET timestamp`, session time-zone variables, and protocol metadata details
remain deferred.
