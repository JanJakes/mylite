# Current temporal scalar functions

## Scope

This feature implements MySQL 8.4.9-compatible current date/time scalar
functions for the expression contexts MyLite already executes:

- `NOW([fsp])`
- `CURRENT_TIMESTAMP`, `CURRENT_TIMESTAMP()`, `CURRENT_TIMESTAMP(fsp)`
- `LOCALTIME`, `LOCALTIME()`, `LOCALTIME(fsp)`
- `LOCALTIMESTAMP`, `LOCALTIMESTAMP()`, `LOCALTIMESTAMP(fsp)`
- `CURDATE()`, `CURRENT_DATE`, `CURRENT_DATE()`
- `CURTIME([fsp])`, `CURRENT_TIME`, `CURRENT_TIME()`, `CURRENT_TIME(fsp)`

The functions are supported in no-table `SELECT`, one-table `SELECT`
projection, `WHERE`, and `ORDER BY`, and supported single-table `UPDATE` and
`DELETE` expression paths.

Out of scope:

- `DATE()`, `DATE_ADD()`, `ADDDATE()`, `DATE_SUB()`, and other temporal
  arithmetic/extraction functions
- named time zones, mutable `@@time_zone`, and time-zone table integration
- `SET timestamp` and other system-variable-controlled clock overrides
- automatic `ON UPDATE CURRENT_TIMESTAMP` column refresh
- protocol-level binary temporal value encoding beyond the existing metadata
  descriptors

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- MySQL 8.4 Reference Manual, Fractional Seconds in Time Values:
  https://dev.mysql.com/doc/refman/8.4/en/fractional-seconds.html
- MySQL 8.4 Reference Manual, DATE, DATETIME, and TIMESTAMP Types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/utc-temporal-functions/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/temporal-column-types/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/delete-single-table/specs.md`

Runtime behavior was verified against the official `mysql:8.4.9` Docker image
in container `mylite-mysql-849`, using:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 Behavior

MySQL evaluates current date/time functions once at statement start. Repeated
references in a query, including synonyms such as `NOW()` and
`CURRENT_TIMESTAMP`, return the same timestamp. `CURDATE()` and `CURTIME()`
derive their date and time components from that same statement timestamp.

The returned value is in the session time zone. The verified runtime produced:

| Setup | Expression | Result |
| --- | --- | --- |
| `SET time_zone = '+00:00'; SET timestamp = 1700000000` | `NOW()` | `2023-11-14 22:13:20` |
| same | `CURRENT_TIMESTAMP(3)` | `2023-11-14 22:13:20.000` |
| same | `LOCALTIME(2)` | `2023-11-14 22:13:20.00` |
| same | `LOCALTIMESTAMP(4)` | `2023-11-14 22:13:20.0000` |
| same | `CURDATE()` / `CURRENT_DATE` | `2023-11-14` |
| same | `CURTIME(5)` / `CURRENT_TIME(5)` | `22:13:20.00000` |
| `SET time_zone = '+02:30'; SET timestamp = 1700000000` | `NOW()` | `2023-11-15 00:43:20` |

With `SET timestamp = 1700000000.987654`, MySQL truncates the timestamp's
microsecond part to the requested `fsp` display precision:

| Expression | Result |
| --- | --- |
| `NOW(0)` | `2023-11-14 22:13:20` |
| `NOW(1)` | `2023-11-14 22:13:20.9` |
| `NOW(3)` | `2023-11-14 22:13:20.987` |
| `NOW(6)` | `2023-11-14 22:13:20.987654` |
| `CURTIME(3)` | `22:13:20.987` |

The accepted fractional-seconds precision is an unsigned integer literal from
0 through 6. Omitted `fsp` defaults to 0. MySQL rejects too-large precision with
error 1426 and rejects negative, string, `NULL`, decimal, expression, or
multi-argument forms as syntax errors.

Bare forms are accepted only for the SQL-standard/current keywords MySQL
documents. `CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_TIMESTAMP`, `LOCALTIME`,
and `LOCALTIMESTAMP` are expressions. Bare `CURDATE`, `CURTIME`, and `NOW` are
ordinary identifiers and fail as unknown columns outside a context where such a
column exists.

Observed result metadata:

| Expression family | Field type | Length at `fsp=0` | Length at `fsp=n>0` | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `NOW`, `CURRENT_TIMESTAMP`, `LOCALTIME`, `LOCALTIMESTAMP` | `DATETIME` | 19 | `20 + n` | `n` | `NOT_NULL BINARY` |
| `CURDATE`, `CURRENT_DATE` | `DATE` | 10 | 10 | 0 | `NOT_NULL BINARY` |
| `CURTIME`, `CURRENT_TIME` | `TIME` | 8 | `9 + n` | `n` | `NOT_NULL BINARY` |

## MyLite Compatibility Decisions

MyLite does not yet expose mutable session time-zone variables. The first slice
therefore treats UTC as the effective session time zone for these functions.
This matches the existing `CURRENT_TIMESTAMP` default implementation, which
formats UTC time, and avoids host-local time zone variability. Full
`@@time_zone`, named-zone tables, and `SET timestamp` overrides are deferred.

Each prepared statement owns a lazy statement timestamp. The first current
temporal function evaluation captures the current UTC wall-clock instant with
microsecond precision where the platform exposes it. All later current
temporal evaluations in that statement use the cached instant, including
per-row evaluation in table scans, `WHERE`, `ORDER BY`, `UPDATE`, and `DELETE`.

Fractional precision formatting truncates the cached microseconds to the
requested number of digits. It never rounds into the next second. When `fsp` is
0, no decimal point is emitted.

Wrong arity and invalid `fsp` should be rejected through MyLite's current
parser/binder diagnostic policy. Exact native diagnostics such as MySQL error
1426 for `NOW(7)` are deferred until parser/runtime errors carry the full
native code and SQLSTATE surface for scalar built-ins.

## MyLite Lemon Grammar Snippets

These snippets describe the intended MyLite grammar shape; they are not copied
from MySQL grammar.

```lemon
primary_expression ::= current_timestamp_value.
primary_expression ::= bare_current_temporal_function.
primary_expression ::= scalar_function_call.

current_timestamp_value ::= CURRENT_TIMESTAMP.
current_timestamp_value ::= CURRENT_TIMESTAMP LPAREN RPAREN.
current_timestamp_value ::= CURRENT_TIMESTAMP LPAREN temporal_fsp RPAREN.

bare_current_temporal_function ::= CURRENT_DATE.
bare_current_temporal_function ::= CURRENT_TIME.
bare_current_temporal_function ::= LOCALTIME.
bare_current_temporal_function ::= LOCALTIMESTAMP.

scalar_function_call ::= function_name LPAREN RPAREN.
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

/* Binder/parser validation limits these names to zero or one integer fsp. */
current_datetime_function_name ::= NOW.
current_datetime_function_name ::= LOCALTIME.
current_datetime_function_name ::= LOCALTIMESTAMP.

current_time_function_name ::= CURTIME.
current_time_function_name ::= CURRENT_TIME.

current_date_function_name ::= CURDATE.
current_date_function_name ::= CURRENT_DATE.

temporal_fsp ::= INTEGER. /* valid values are 0..6 */
```

## Runtime Semantics

`NOW`, `CURRENT_TIMESTAMP`, `LOCALTIME`, and `LOCALTIMESTAMP` produce a
non-`NULL` `DATETIME` text value:

```text
YYYY-MM-DD hh:mm:ss[.fraction]
```

`CURDATE` and `CURRENT_DATE` produce a non-`NULL` `DATE` text value:

```text
YYYY-MM-DD
```

`CURTIME` and `CURRENT_TIME` produce a non-`NULL` `TIME` text value:

```text
hh:mm:ss[.fraction]
```

The value model remains text-backed for execution because MyLite's scalar
runtime currently returns C API text for scalar results. Metadata must still
advertise `DATE`, `TIME`, or `DATETIME` field types with binary charset id 63,
binary flag, not-null flag, the lengths above, and `decimals = fsp`.

## Tests

Parser tests:

- parse all accepted spellings, including bare standard forms, empty
  parentheses, and `fsp` 0 through 6 where supported
- reject bare `NOW`, bare `CURDATE`, and bare `CURTIME` as ordinary
  identifiers only at runtime/name resolution, not as current temporal
  functions
- reject `NOW(7)`, `NOW(-1)`, `NOW('3')`, `NOW(NULL)`, `NOW(1+1)`,
  `NOW(1,2)`, `CURTIME(7)`, `CURRENT_TIME(7)`, `LOCALTIME(7)`,
  `LOCALTIMESTAMP(7)`, `CURDATE(0)`, and `CURRENT_DATE(0)` according to
  MyLite parser/binder policy
- keep the UTC current temporal functions covered by
  `docs/specs/utc-temporal-functions/specs.md`

Runtime tests:

- no-table `SELECT` returns one statement-stable value across synonyms
- `fsp` values 0 through 6 produce expected string lengths and fraction digit
  counts without exact wall-clock equality assumptions
- result metadata matches MySQL-observed `DATETIME`, `DATE`, and `TIME`
  descriptors
- aliases and default expression labels continue to work
- one-table `SELECT` projection, `WHERE`, and `ORDER BY` can evaluate these
  functions
- single-table `UPDATE` can assign values from `NOW(6)`, `CURDATE()`, and
  `CURTIME(6)`
- single-table `DELETE` can use current temporal predicates
- unsupported UTC variants and invalid arity/fsp forms fail deterministically

## Compatibility Status

After implementation, the scoped functions are `🟡` rather than `✅` because
the first slice deliberately fixes the effective session time zone to UTC,
defers `SET timestamp`, and reports invalid `fsp` through MyLite's current
diagnostic surface instead of exact MySQL 1426/1064 details.
