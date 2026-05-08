# TIMESTAMPDIFF() scalar function

## Scope

This feature implements the first MySQL 8.4.9-compatible
`TIMESTAMPDIFF(unit, datetime_expr1, datetime_expr2)` slice for expression
contexts MyLite already executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` and `DELETE` expression paths

The supported units in this slice are:

- `DAY`
- `WEEK`
- `MONTH`
- `YEAR`
- `HOUR`
- `MINUTE`
- `SECOND`
- `QUARTER`
- `MICROSECOND`

The MySQL-supported `SQL_TSI_YEAR`, `SQL_TSI_QUARTER`, `SQL_TSI_MONTH`,
`SQL_TSI_WEEK`, `SQL_TSI_DAY`, `SQL_TSI_HOUR`, `SQL_TSI_MINUTE`, and
`SQL_TSI_SECOND` aliases are accepted and normalized to the corresponding
unit. MySQL 8.4.9 rejects `SQL_TSI_MICROSECOND`, and MyLite rejects it too.

Out of scope:

- combined interval units such as `YEAR_MONTH`, which MySQL rejects for
  `TIMESTAMPDIFF()`
- time-only input conversion
- named time zones and `TIMESTAMP` session time-zone conversion
- exact native parser diagnostic text and native scalar arity error-code parity
- overflow diagnostics at the edges of MySQL's temporal range

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- MySQL 8.4 Reference Manual, Date and Time Literals:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html>
- MySQL 8.4 Reference Manual, DATE, DATETIME, and TIMESTAMP Types:
  <https://dev.mysql.com/doc/refman/8.4/en/datetime.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/date-and-datediff-functions/specs.md`
  - `docs/specs/date-add-sub-functions/specs.md`
  - `docs/specs/temporal-part-functions/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`

Runtime behavior was verified against the official `mysql:8.4.9` Docker image
in container `mylite-mysql-849`, using:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 behavior

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

`TIMESTAMPDIFF()` returns a signed integer for `datetime_expr2 -
datetime_expr1` in the requested unit. Date-only values are treated as
datetimes at `00:00:00`.

Verified results for this slice:

| Expression | Result | Warnings |
| --- | ---: | --- |
| `TIMESTAMPDIFF(DAY,'2024-02-28','2024-03-01')` | `2` | none |
| `TIMESTAMPDIFF(DAY,'2024-02-28 23:59:59','2024-02-29 00:00:00')` | `0` | none |
| `TIMESTAMPDIFF(WEEK,'2024-02-01','2024-02-15')` | `2` | none |
| `TIMESTAMPDIFF(WEEK,'2024-01-01','2024-01-14')` | `1` | none |
| `TIMESTAMPDIFF(MONTH,'2024-01-31','2024-02-29')` | `0` | none |
| `TIMESTAMPDIFF(MONTH,'2024-02-29','2025-02-28')` | `11` | none |
| `TIMESTAMPDIFF(MONTH,'2024-02-29','2025-03-01')` | `12` | none |
| `TIMESTAMPDIFF(YEAR,'2020-02-29','2024-02-28')` | `3` | none |
| `TIMESTAMPDIFF(YEAR,'2020-02-29','2024-02-29')` | `4` | none |
| `TIMESTAMPDIFF(HOUR,'2024-02-28 00:00:00','2024-02-29 12:00:00')` | `36` | none |
| `TIMESTAMPDIFF(MINUTE,'2024-02-28 00:00:30','2024-02-28 00:02:29')` | `1` | none |
| `TIMESTAMPDIFF(SECOND,'2024-02-28 00:00:00','2024-02-28 00:00:01.900000')` | `1` | none |
| `TIMESTAMPDIFF(MICROSECOND,'2024-01-01 00:00:00.000001','2024-01-01 00:00:01.000003')` | `1000002` | none |
| `TIMESTAMPDIFF(QUARTER,'2024-01-31','2024-10-30')` | `2` | none |
| `TIMESTAMPDIFF(QUARTER,'2024-01-31','2024-10-31')` | `3` | none |
| `TIMESTAMPDIFF(SQL_TSI_DAY,'2024-01-01','2024-01-03')` | `2` | none |
| `TIMESTAMPDIFF(SQL_TSI_QUARTER,'2024-01-01','2024-07-01')` | `2` | none |
| `TIMESTAMPDIFF(SECOND,'2024-01-01 00:00:00.900000','2024-01-01 00:00:01.000000')` | `0` | none |
| `TIMESTAMPDIFF(MINUTE,'2024-01-01 00:00:00.900000','2024-01-01 00:01:00.000000')` | `0` | none |

Negative differences are truncated toward zero at the requested unit boundary:

| Expression | Result |
| --- | ---: |
| `TIMESTAMPDIFF(DAY,'2024-03-01','2024-02-29')` | `-1` |
| `TIMESTAMPDIFF(HOUR,'2024-01-02 01:59:59','2024-01-01 00:00:00')` | `-25` |
| `TIMESTAMPDIFF(SECOND,'2024-01-01 00:00:01.000000','2024-01-01 00:00:00.900000')` | `0` |
| `TIMESTAMPDIFF(SECOND,'2024-01-01 00:00:01.900000','2024-01-01 00:00:00.000000')` | `-1` |
| `TIMESTAMPDIFF(MONTH,'2024-02-29','2024-01-31')` | `0` |
| `TIMESTAMPDIFF(MONTH,'2025-02-28','2024-02-29')` | `-11` |
| `TIMESTAMPDIFF(YEAR,'2025-02-28','2024-02-29')` | `0` |
| `TIMESTAMPDIFF(YEAR,'2001-03-01','2000-02-29')` | `-1` |
| `TIMESTAMPDIFF(MICROSECOND,'2024-01-01 00:00:01.000003','2024-01-01 00:00:00.000001')` | `-1000002` |
| `TIMESTAMPDIFF(QUARTER,'2024-10-31','2024-01-31')` | `-3` |

`MONTH` and `YEAR` count complete month or year boundaries. They are not
derived from fixed day counts.

`NULL` and invalid temporal values:

| Expression | Result | Warnings |
| --- | --- | --- |
| `TIMESTAMPDIFF(DAY,NULL,'2024-01-01')` | `NULL` | none |
| `TIMESTAMPDIFF(DAY,'2024-01-01',NULL)` | `NULL` | none |
| `TIMESTAMPDIFF(DAY,'bad','2024-01-01')` | `NULL` | 1292 incorrect datetime value |
| `TIMESTAMPDIFF(DAY,'2024-01-01','bad')` | `NULL` | 1292 incorrect datetime value |
| `TIMESTAMPDIFF(DAY,'bad','worse')` | `NULL` | one 1292 warning for the first invalid operand |
| `TIMESTAMPDIFF(DAY,'bad',NULL)` | `NULL` | one 1292 warning for `'bad'` |
| `TIMESTAMPDIFF(DAY,NULL,'bad')` | `NULL` | none |
| `TIMESTAMPDIFF(DAY,'0000-00-00','2024-01-01')` | `NULL` | 1292 incorrect datetime value |
| `TIMESTAMPDIFF(DAY,20240229.9,20240301.9)` | `1` | two 1292 truncated date warnings |

The supported input conversion surface is the same as MyLite's existing
complete-date temporal parser: dashed date/datetime strings, compact strings,
integer compact temporal values, approximate numeric values truncated to their
integer temporal text with warning 1292 when fractional digits are nonzero,
two-digit year normalization, fractional seconds, and values produced by
current temporal and date-arithmetic functions.

Malformed syntax:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT TIMESTAMPDIFF(YEAR_MONTH,'2024-01-01','2024-02-01')` | syntax error 1064 |
| `SELECT TIMESTAMPDIFF(SQL_TSI_MICROSECOND,'2024-01-01','2024-01-02')` | syntax error 1064 |
| `SELECT TIMESTAMPDIFF('DAY','2024-01-01','2024-01-02')` | syntax error 1064 |
| `SELECT TIMESTAMPDIFF(DAY,'2024-01-01')` | syntax error 1064 |
| `SELECT TIMESTAMPDIFF(DAY,'2024-01-01','2024-01-02','x')` | syntax error 1064 |

Observed metadata for supported units:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| `TIMESTAMPDIFF(...)` | `LONGLONG` | `21` | `0` | binary (63) | `BINARY NUM` | yes |

## MyLite compatibility decisions

MyLite stores the interval unit as metadata on the function-call AST node. The
argument list contains exactly the two datetime expressions, preserving the
existing rule that argument-list children are evaluatable expressions.

The supported simple units listed in scope are accepted as interval-unit
keywords. MySQL's `SQL_TSI_` aliases are accepted for all observed units except
`SQL_TSI_MICROSECOND`, which MySQL rejects. Combined interval units remain
rejected, matching MySQL for `YEAR_MONTH`.

For invalid temporal values, MyLite should match the existing temporal parser's
warning text family and default strict-mode warning policy. In `UPDATE` and
`DELETE`, warnings from invalid `TIMESTAMPDIFF()` operands are promoted through
MyLite's existing DML warning-promotion path so the statement fails and changes
are rolled back.

The first implementation intentionally preserves the observed MySQL
short-circuit conversion behavior for this function: convert the first
non-`NULL` operand before evaluating and converting the second. If the first
operand is `NULL`, the second operand is not converted for temporal warnings.

## MyLite Lemon grammar snippets

These snippets describe the intended MyLite grammar shape; they are not copied
from MySQL grammar.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.

interval_unit ::= DAY.
interval_unit ::= WEEK.
interval_unit ::= MONTH.
interval_unit ::= YEAR.
interval_unit ::= HOUR.
interval_unit ::= MINUTE.
interval_unit ::= SECOND.
interval_unit ::= QUARTER.
interval_unit ::= MICROSECOND.
```

When `function_name` text is `TIMESTAMPDIFF`, MyLite validates that the
argument list has exactly three parsed arguments, that the first argument is one
of the supported interval-unit identifiers above or a supported `SQL_TSI_`
alias, and then stores the unit as function-call metadata while preserving the
two datetime expressions as runtime arguments. `TIMESTAMPDIFF` remains a
nonreserved identifier when it is not used as a call name.

## Runtime semantics

1. Evaluate `datetime_expr1`.
2. If `datetime_expr1` is `NULL`, return `NULL` without converting
   `datetime_expr2`.
3. Convert `datetime_expr1` to a complete date/datetime. On failure, append
   warning 1292 and return `NULL`.
4. Evaluate `datetime_expr2`.
5. If `datetime_expr2` is `NULL`, return `NULL`.
6. Convert `datetime_expr2` to a complete date/datetime. On failure, append
   warning 1292 and return `NULL`.
7. Compute `datetime_expr2 - datetime_expr1` in the requested unit and return a
   signed integer.

`DAY`, `WEEK`, `HOUR`, `MINUTE`, `SECOND`, and `MICROSECOND` use complete
elapsed boundaries from day, second, and microsecond arithmetic. `MONTH`,
`QUARTER`, and `YEAR` use calendar boundary logic so partial final months,
quarters, or years are not counted. Negative results use the same truncation
direction observed in MySQL.

The feature has no file-format, schema-catalog, or SQLite storage impact.

## Tests

Parser tests:

- `TIMESTAMPDIFF(DAY|WEEK|MONTH|YEAR|HOUR|MINUTE|SECOND|QUARTER|MICROSECOND, expr, expr)`
- nested temporal inputs such as `TIMESTAMPDIFF(DAY, CURDATE(), DATE(NOW()))`
- `timestampdiff` as a nonreserved column identifier
- parser rejection for quoted units, missing arguments, extra arguments,
  `YEAR_MONTH`, and MySQL-rejected `SQL_TSI_MICROSECOND`

Runtime tests:

- no-table scalar results for all supported units
- leap-day, month-end, negative, and boundary truncation cases
- `NULL` propagation and invalid temporal warning behavior
- current temporal and nested date-arithmetic inputs
- compact string, integer, approximate numeric, and fractional datetime inputs
- result metadata: nullable signed `LONGLONG`, length 21, binary charset,
  `BINARY NUM`
- one-table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignments, predicates, and order keys
- supported `DELETE` predicates and order keys
- DML warning promotion and rollback for invalid operands
- unsupported units and malformed arity/syntax

## Compatibility status

After implementation, `TIMESTAMPDIFF()` has partial compatibility for the
supported scalar expression paths and supported simple units. Remaining gaps
are time-only input conversion, exact native diagnostics, and broader temporal
storage/time-zone semantics.
