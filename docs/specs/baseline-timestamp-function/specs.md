# Baseline TIMESTAMP() Function

## Summary

This phase adds a narrow `TIMESTAMP()` scalar-function slice. The function is
supported in no-source `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection. It does not add DML assignment support,
predicates, defaults, generated columns, or general temporal expression
coercion.

Compatibility authority is the MySQL 8.4 Reference Manual date and time
functions page (`https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html`)
and MySQL 8.4.9 runtime probes against the local `mylite-mysql-849`
container.

## Runtime Evidence

The following MySQL 8.4.9 probes define the supported and intentionally
deferred behavior for this slice:

```sh
printf '%s\n' \
  "SET time_zone='+00:00'; SET sql_mode=''; \
   SELECT TIMESTAMP('2003-12-31'), \
          TIMESTAMP('2003-12-31 12:34:56'), \
          TIMESTAMP('2003-12-31','12:00:00'), \
          TIMESTAMP('2003-12-31','1 02:03:04'), \
          TIMESTAMP(NULL), TIMESTAMP('bad'); SHOW WARNINGS;" \
  | docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
      --batch --raw --skip-column-names
```

Observed:

- `TIMESTAMP('2003-12-31')` returns `2003-12-31 00:00:00`.
- `TIMESTAMP('2003-12-31 12:34:56')` returns the same datetime.
- `TIMESTAMP('2003-12-31','12:00:00')` returns
  `2003-12-31 12:00:00`.
- `TIMESTAMP('2003-12-31','1 02:03:04')` returns
  `2004-01-01 02:03:04`.
- any `NULL` argument returns `NULL`.
- invalid first datetime strings return `NULL` and warning `1292 /
  22007`, with message shape `Incorrect datetime value: '...'`.
- when the first argument is invalid, MySQL reports the first-argument
  datetime warning before considering a `NULL`, invalid, or clipped second
  argument; when the first argument is `NULL`, second-argument errors are not
  reported.

```sh
printf '%s\n' \
  "SET time_zone='+00:00'; SET sql_mode=''; \
   SELECT TIMESTAMP('2003-12-31','-01:02:03'), \
          TIMESTAMP('2003-12-31','838:59:59'), \
          TIMESTAMP('2003-12-31','839:00:00'), \
          TIMESTAMP('9999-12-31 23:59:59','00:00:01'); SHOW WARNINGS;" \
  | docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
      --batch --raw --skip-column-names
```

Observed:

- negative time arguments subtract seconds from the date/datetime argument;
- time arguments above MySQL's `TIME` range are clipped to `838:59:59` and
  warning `1292 / 22007`, with message shape
  `Truncated incorrect time value: '...'`.
- arithmetic above MySQL's supported datetime range returns `NULL` and warning
  `1441 / HY000`, with message shape
  `Datetime function: add_time field overflow`.

```sh
printf '%s\n' \
  "DROP DATABASE IF EXISTS mylite_timestamp_probe; \
   CREATE DATABASE mylite_timestamp_probe; USE mylite_timestamp_probe; \
   CREATE TABLE t(id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL, tm TIME, v VARCHAR(32)); \
   INSERT INTO t VALUES \
     (1,'2003-12-31','2003-12-31 12:34:56','2003-12-31 10:00:00','01:02:03','2003-12-31 06:00:00'), \
     (2,NULL,NULL,NULL,NULL,NULL); \
   SELECT id, TIMESTAMP(d), TIMESTAMP(dt), TIMESTAMP(ts), TIMESTAMP(v), \
          TIMESTAMP(d, tm), TIMESTAMP(dt, tm) FROM t ORDER BY id;" \
  | docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
      --batch --raw --skip-column-names
```

Observed row-backed descriptor values:

- `DATE`, `DATETIME`, and `TIMESTAMP` first arguments are accepted;
- `TIME` second arguments are accepted;
- string columns containing canonical datetime text are accepted;
- `NULL` descriptor values propagate to `NULL`.

```sh
for sql in \
  "SELECT TIMESTAMP();" \
  "SELECT TIMESTAMP('2001-01-01','00:00:00','x');" \
  "SELECT TIMESTAMP(1);" \
  "SELECT TIMESTAMP('2001-01-01', 1);"
do
  printf '%s\n' "$sql" \
    | docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
        --batch --raw --skip-column-names
done
```

Observed:

- zero and three arguments are syntax errors in MySQL's `TIMESTAMP` grammar;
- numeric temporal/date/time coercion is accepted by MySQL but is deferred in
  this MyLite slice.

## Supported Syntax

Independently authored MyLite Lemon-syntax shape:

```lemon
expression(A) ::= TIMESTAMP(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= TIMESTAMP(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
```

Zero-argument and more-than-two-argument forms should parse to deterministic
function-argument diagnostics where the existing parser scaffolding permits.

## Semantics

`TIMESTAMP(value)`:

- returns `NULL` when `value` is `NULL`;
- accepts canonical `YYYY-MM-DD` strings and descriptor `DATE` values,
  returning `YYYY-MM-DD 00:00:00`;
- accepts canonical `YYYY-MM-DD HH:MM:SS` strings and descriptor `DATETIME`,
  `TIMESTAMP`, and nonbinary string-family values containing that shape,
  returning the same datetime;
- returns `NULL` and warning `1292 / 22007` for admitted but invalid string
  values.

`TIMESTAMP(value, time_value)`:

- returns `NULL` when either argument is `NULL`;
- evaluates the first argument as above;
- validates the first argument before applying `time_value` nullability or
  time parsing, matching MySQL's observed warning order;
- accepts canonical `[-]HH:MM:SS`, `[-]HHH:MM:SS`, and
  `[sign]days HH:MM:SS` string time values plus descriptor `TIME` and
  nonbinary string-family values containing those shapes;
- clips time values beyond `838:59:59` / `-838:59:59` with warning
  `1292 / 22007` before adding them to the first argument;
- returns `NULL` and warning `1292 / 22007` for admitted but invalid time
  values;
- returns `NULL` and warning `1441 / HY000` if adding the time value produces a
  datetime outside the currently supported `1000-01-01 00:00:00` through
  `9999-12-31 23:59:59` physical range. The supported overflow diagnostic
  message follows MySQL's `TIMESTAMP()` shape:
  `Datetime function: add_time field overflow`.

This slice intentionally keeps MyLite's supported output in second precision.
MySQL accepts and preserves fractional seconds in several `TIMESTAMP()` inputs;
those remain unsupported.

## Deferred Behavior

The following MySQL behavior is intentionally not admitted yet:

- numeric date/time coercion such as `TIMESTAMP(20031231)` or
  `TIMESTAMP('2001-01-01', 1)`;
- compact temporal strings such as `20031231`;
- fractional seconds;
- `TIME` as the first argument;
- arbitrary expressions, parameters, subqueries, aggregates, grouped queries,
  predicates, ordering expressions, DML assignment values, generated columns,
  defaults, casts, and temporal literal introducers;
- named time-zone behavior and `TIMESTAMP` storage/readback conversion beyond
  the current fixed-UTC descriptor subset.

## Architecture

- Public API: no ABI change. Results flow through existing `mylite_execute()`
  and result APIs.
- Parser/AST: add explicit `TIMESTAMP()` AST nodes for one and two arguments
  plus an argument-count diagnostic node.
- Analyzer/planner: no-source scalar planning uses the existing session-scalar
  evaluator. Row-scalar planning resolves descriptor columns through the MyLite
  catalog and rejects unknown or unsupported columns before generating SQLite
  SQL.
- Catalog: descriptors remain authoritative for table-backed column
  resolution. SQLite schema text is not consulted for logical types.
- Runtime: temporal conversion and warnings live in MyLite-owned runtime code.
  Row-scalar table queries call a MyLite-registered SQLite scalar function via
  the public SQLite extension API; SQLite only invokes the function and stores
  row values.
- Storage/VFS: no file-format, VFS, preamble, or SQLite fork change is needed.

## Diagnostics

- Missing or extra arguments: MySQL-style native function parameter-count
  diagnostic where parsed by MyLite.
- Unknown no-source identifiers: existing unknown-column diagnostic.
- Unknown row-backed columns: existing descriptor column diagnostic.
- Unsupported argument types: MyLite-specific unsupported diagnostic.
- Invalid first temporal values: warning `1292 / 22007`,
  `Incorrect datetime value: '...'`, result `NULL`.
- Invalid or clipped second temporal values: warning `1292 / 22007`,
  `Truncated incorrect time value: '...'`, result `NULL` for invalid values or
  clipped addition for out-of-range time values.
- Datetime addition overflow: warning `1441 / HY000`,
  `Datetime function: add_time field overflow`, result `NULL`.
- Allocation failure: existing `MYLITE_NOMEM` / `HY001`.

## Tests

Add MySQL-runtime expectation checks and plain C runtime tests covering:

- no-source and `FROM DUAL` one-arg and two-arg results;
- `DO TIMESTAMP(...)` row-count and warning-count behavior;
- `NULL` propagation;
- invalid first and second string warnings;
- negative time, day-hour time, clipping, and overflow boundaries;
- single-table row-scalar projection over `DATE`, `DATETIME`, `TIMESTAMP`,
  `TIME`, and nonbinary string descriptors;
- reopen persistence of stored descriptor rows observed through
  `TIMESTAMP()`;
- deterministic diagnostics for wrong arity, unknown columns, numeric coercion,
  `TIME` as first argument, and fractional values.
