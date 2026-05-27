# Baseline DATE Interval Core Units

## Goal

Extend the current MyLite `DATE_ADD()` / `DATE_SUB()` / `ADDDATE()` /
`SUBDATE()` temporal arithmetic slice from `SECOND` only to the common
single-part MySQL interval units used by application SQL:

```sql
DATE_ADD(value, INTERVAL interval_value MINUTE)
DATE_SUB(value, INTERVAL interval_value YEAR)
ADDDATE(value, INTERVAL interval_value MONTH)
SUBDATE(value, INTERVAL interval_value DAY)
```

This remains a narrow scalar and row-scalar projection feature. It is not a
general temporal expression engine, and it does not make temporal arithmetic
available in predicates, ordering/grouping expressions, DML assignments,
defaults, generated columns, joins, subqueries, or arbitrary SQLite pass-through.

## Sources

- Official MySQL 8.4 date and time function documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 expression and temporal interval documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Official MySQL 8.4 function-name parsing rules:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Observed MySQL 8.4.9 runtime behavior from the local
  `mylite-mysql-849` container. The implementation test artifact is
  `packages/libmylite/tests/mysql_baseline_date_interval_core_units_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for the admitted subset:

- Datetime input returns datetime text for every supported unit.
- Date-only input returns datetime text for time-bearing units `SECOND`,
  `MINUTE`, and `HOUR`.
- Date-only input returns date text for date-bearing units `DAY`, `WEEK`,
  `MONTH`, `QUARTER`, and `YEAR`.
- `DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 MINUTE)` returns
  `2008-01-02 13:30:17`; `HOUR`, `DAY`, `WEEK`, `MONTH`, `QUARTER`, and `YEAR`
  similarly add the named unit.
- `DATE_ADD('2008-01-02', INTERVAL 1 HOUR)` returns
  `2008-01-02 01:00:00`, while `DATE_ADD('2008-01-02', INTERVAL 1 DAY)` returns
  `2008-01-03`.
- Calendar-month arithmetic clamps the day to the last valid day in the target
  month: `2024-01-31 + INTERVAL 1 MONTH` returns `2024-02-29`,
  `2023-01-31 + INTERVAL 1 MONTH` returns `2023-02-28`, and
  `2024-02-29 + INTERVAL 1 YEAR` returns `2025-02-28`.
- `DATE_SUB()` and `SUBDATE()` subtract the interval; negative intervals add.
  `ADDDATE()` matches `DATE_ADD()` for the admitted interval form.
- `INTERVAL NULL unit` or a `NULL` temporal value returns `NULL`.
- Exact quoted integer interval strings such as `'2'`, `'+2'`, `'-2'`, and
  `'02'` are accepted with no warnings.
- MySQL also accepts truncating interval strings such as `'2x'` with warning
  `1292`; MyLite does not admit warning-producing prefix interval conversion in
  this slice.
- MySQL can produce pre-1000 dates for some date-unit arithmetic. MyLite keeps
  the existing nonzero temporal arithmetic output range
  `1000-01-01` through `9999-12-31 23:59:59` for this baseline and returns a
  deterministic unsupported diagnostic for lower results.
- Scalar literal results expose string-like metadata. Row-scalar descriptor
  `DATE` inputs expose `DATE` metadata for date-bearing units and `DATETIME`
  metadata for time-bearing units; descriptor `DATETIME` and `TIMESTAMP` inputs
  expose `DATETIME` metadata; descriptor string-family inputs expose
  string-like metadata.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row-scalar envelope:
  source table, optional existing `WHERE`, descriptor-column `ORDER BY`, and
  existing `LIMIT`;
- `DATE_ADD(value, INTERVAL interval_value unit)`;
- `DATE_SUB(value, INTERVAL interval_value unit)`;
- `ADDDATE(value, INTERVAL interval_value unit)`;
- `SUBDATE(value, INTERVAL interval_value unit)`;
- supported units:
  - `YEAR`;
  - `QUARTER`;
  - `MONTH`;
  - `WEEK`;
  - `DAY`;
  - `HOUR`;
  - `MINUTE`;
  - `SECOND`;
- parsed but rejected unit:
  - `MICROSECOND`, because fractional-second result formatting is not part of
    this baseline;
- `interval_value` as:
  - a decimal integer literal with optional unary `+` or `-`;
  - an exact quoted decimal integer string with optional unary `+` or `-`;
  - `NULL`;
- temporal `value` as:
  - `NULL`;
  - a single- or double-quoted canonical date string `YYYY-MM-DD`;
  - a single- or double-quoted canonical datetime string
    `YYYY-MM-DD HH:MM:SS`;
  - in row-scalar `SELECT`, a descriptor column whose family is `DATE`,
    `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline `TEXT`;
- complete, valid nonzero temporal values in the current MyLite arithmetic
  range;
- `DATE_SUB()` / `SUBDATE()` subtraction by negating the interval with checked
  overflow handling;
- stable MyLite-owned temporal arithmetic for second, minute, hour, day, week,
  month, quarter, and year units;
- result text through existing public result APIs, with row-scalar metadata
  following the admitted MySQL-observed `DATE` / `DATETIME` / string split.

## Deferred Surface

This slice intentionally does not support:

- `MICROSECOND` arithmetic, fractional seconds, or fractional metadata;
- composite interval units such as `DAY_SECOND`, `HOUR_MINUTE`, or
  `YEAR_MONTH`;
- `SQL_TSI_` unit aliases for this function family;
- interval expressions, column intervals, boolean intervals, decimal/floating
  intervals, warning-producing prefix string interval conversion, parameters,
  user variables, or subqueries;
- `ADDDATE(date, days)` or non-interval `SUBDATE` forms;
- relaxed temporal input strings, incomplete dates, zero dates, numeric temporal
  coercion, time-zone conversion, or full `TIMESTAMP` session conversion;
- `TIME` descriptor arguments or time-only strings;
- predicates, ordering keys, grouping expressions, DML assignments, defaults,
  generated columns, check constraints, or arbitrary expression evaluation.

## Grammar

MyLite admits this independently authored parser shape:

```lemon
date_interval_unit(A) ::= YEAR(T).
date_interval_unit(A) ::= QUARTER(T).
date_interval_unit(A) ::= MONTH(T).
date_interval_unit(A) ::= WEEK(T).
date_interval_unit(A) ::= DAY(T).
date_interval_unit(A) ::= HOUR(T).
date_interval_unit(A) ::= MINUTE(T).
date_interval_unit(A) ::= SECOND(T).
date_interval_unit(A) ::= MICROSECOND(T). /* parsed for deterministic rejection */

expression(A) ::=
    DATE_ADD(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R).
expression(A) ::=
    DATE_SUB(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R).
expression(A) ::=
    ADDDATE(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R).
expression(A) ::=
    SUBDATE(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R).
```

The function-call productions keep the existing whitespace-sensitive native
function behavior for `DATE_ADD` and `DATE_SUB`: without `IGNORE_SPACE`, the
function token and opening parenthesis must be adjacent; with `IGNORE_SPACE`,
intervening whitespace is accepted.

## Runtime Semantics

Planning:

1. Validate the function shape and resolve the unit identifier.
2. Reject `MICROSECOND` and non-admitted units with deterministic diagnostics.
3. Decode the temporal argument using the existing string-literal and descriptor
   resolution rules.
4. Decode the interval argument as signed 64-bit integer or exact quoted signed
   integer string, or mark it `NULL`.
5. In row-scalar projection, lower to SQLite SQL over stable physical table
   names and quoted physical column names. The temporal value, input-kind
   discriminator, interval value, unit discriminator, and operation
   discriminator are bound values; generated SQL never interpolates user
   literals.

Evaluation:

1. `NULL` temporal values or `NULL` intervals return `NULL`.
2. Date-only values are interpreted as midnight.
3. Time-bearing units add checked whole seconds.
4. Date-bearing day/week units add checked whole days while preserving the time
   portion when the input had one.
5. Month/quarter/year units add checked whole months and clamp the day to the
   last valid day in the target month.
6. Date-only inputs with date-bearing units return `YYYY-MM-DD`; date-only
   inputs with time-bearing units and all datetime inputs return
   `YYYY-MM-DD HH:MM:SS`.
7. Supported in-range expressions produce no warnings.
8. Row-backed invalid temporal values keep the existing row-scalar warning
   behavior: return `NULL` and append warning `1292`; row arithmetic overflow
   returns `NULL` and appends warning `1441`.

The feature is pure scalar evaluation. It does not mutate descriptors,
descriptor caches, catalog generation, `sqlite_schema_generation`, physical
table definitions, user rows, or the `.mylite` preamble.

## Diagnostics

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- wrong function shape:
  `FUNCTION() supports only FUNCTION(date, INTERVAL value unit)`;
- unsupported unit:
  `FUNCTION() supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, and SECOND units`;
- unsupported temporal scalar argument:
  `FUNCTION() supports only date or datetime string literals and NULL`;
- unsupported row temporal argument:
  `FUNCTION() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor columns, string descriptor columns, and NULL`;
- `TIME` row argument:
  `FUNCTION() does not yet support TIME values`;
- embedded NUL in a temporal literal:
  `FUNCTION() date literals do not support NUL bytes`;
- invalid scalar temporal literal:
  `FUNCTION() supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values`;
- unsupported interval value:
  `FUNCTION() INTERVAL unit supports only signed integer literals, exact signed integer string literals, and NULL`;
- interval value outside signed 64-bit range:
  `FUNCTION() INTERVAL unit literals must fit the signed 64-bit range`;
- scalar result outside the supported range:
  `FUNCTION() result is outside the supported datetime range`;
- row invalid temporal value: warning `1292 / 22007` and `NULL` result;
- row result overflow: warning `1441 / HY000` and `NULL` result;
- physical SQLite failures and allocation failures: existing runtime behavior;
- public API misuse: no public API changes.

## Architecture

- Public API: unchanged.
- Statement context: unchanged. Existing diagnostics own row warnings emitted by
  MyLite SQLite callbacks.
- Lexer/parser/AST: replace the hard-coded `SECOND` DATE interval function
  shape with a unit child while keeping the existing function-name spacing rule.
- Analyzer/planner: extend the current date-interval planner to carry a unit
  discriminator and derive row-scalar metadata from input kind plus unit.
- Catalog: read-only descriptor resolution for row-scalar columns only.
- Result builder: existing scalar result and row-scalar result APIs. Row-scalar
  metadata is refined for descriptor `DATE` plus date-bearing units.
- Storage/VFS/file format: unchanged. The feature reads rows only and must not
  touch the MyLite preamble.
- SQLite integration: continue using public SQLite scalar-function registration
  and prepared statement APIs. No SQLite fork patch is needed.

## Tests

Add or extend fast C tests under `packages/libmylite/tests/` and a MySQL 8.4.9
expectation script covering:

- parser acceptance for each admitted unit on all four function names;
- deterministic parser/runtime rejection for `MICROSECOND` and composite units;
- no-source, `FROM DUAL`, `DO`, aliases, `ROW_COUNT()`, and warning counts;
- date and datetime scalar inputs for every admitted unit;
- `DATE_SUB()` / `SUBDATE()` subtraction and negative interval behavior;
- exact quoted signed integer interval strings;
- `NULL` temporal and interval propagation;
- month-end and leap-day clamp behavior;
- unsupported interval expressions, prefix strings, decimals, parameters, and
  out-of-range literals;
- row-scalar projection over descriptor `DATE`, `DATETIME`, `TIMESTAMP`,
  `VARCHAR`, and `TEXT` columns;
- row-scalar metadata for `DATE` + date-bearing unit, `DATE` + time-bearing
  unit, `DATETIME`, `TIMESTAMP`, and string-family inputs;
- row invalid temporal warning and row overflow warning;
- existing `WHERE`, descriptor `ORDER BY`, and `LIMIT` composition;
- unchanged catalog generation, SQLite schema generation, and `.mylite`
  preamble;
- independent file-backed handles where relevant.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-temporal.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/type-system-literals-conversion.md`.
