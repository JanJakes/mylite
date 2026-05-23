# Baseline TIMESTAMPADD SECOND Function

## Goal

Add a narrow `TIMESTAMPADD(unit, interval, datetime_expr)` scalar slice for
whole-second temporal arithmetic:

```sql
SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17') AS output;
```

This phase is deliberately smaller than MySQL's full `TIMESTAMPADD()` surface.
It reuses MyLite's existing MyLite-owned `DATE_ADD(... INTERVAL ... SECOND)`
arithmetic and row-scalar SQLite callback path, while deferring broader units,
fractional seconds, expression intervals, general temporal coercion, predicates,
DML assignments, defaults, generated columns, and arbitrary expression planning.

## Sources

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing MyLite temporal arithmetic designs:
  - `docs/specs/baseline-date-add-second/specs.md`
  - `docs/specs/baseline-row-temporal-interval-second-projection/specs.md`
  - `docs/specs/baseline-timestampdiff-function/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_timestampadd_second_function_expectations.sh`.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17')` returns
  `2008-01-02 13:29:18` with zero warnings.
- `SQL_TSI_SECOND` is accepted as a unit alias and returns the same value as
  `SECOND`.
- Positive, negative, zero, unary-plus, and `NULL` interval arguments are
  accepted. A `NULL` interval or temporal value returns `NULL`.
- Date-only strings are treated as midnight. With a second-based unit, MySQL
  returns datetime text.
- Scalar literal calls expose string result-column metadata with the connection
  collation, 29-character display width, and MySQL's scalar var-string decimal
  marker. Row-backed calls over `DATE`, `DATETIME`, and `TIMESTAMP` descriptor
  columns expose `DATETIME` metadata with binary collation and 19-character
  display width; row-backed calls over supported string descriptor columns expose
  string metadata with the connection collation.
- `TIMESTAMPADD` accepts whitespace before `(` in default SQL mode and is usable
  as an unquoted identifier outside function-call contexts.
- MySQL accepts many units and richer interval expressions, including string,
  decimal, boolean, and arithmetic interval expressions. Those remain out of
  scope for this baseline.
- Full-zero and partial-zero temporal strings return `NULL` and warnings in
  MySQL. This baseline does not admit warning-returning invalid scalar literals
  in the no-source path; row-backed projection reuses the existing
  row-temporal interval callback warning behavior.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- `TIMESTAMPADD(unit, interval_value, temporal_value)` with exactly one unit,
  one interval argument, and one temporal argument;
- supported units:
  - `SECOND`;
  - `SQL_TSI_SECOND`;
- `interval_value` as a decimal integer literal with optional unary `+` or `-`,
  or `NULL`;
- interval integer values that fit the signed 64-bit range;
- `temporal_value` as:
  - `NULL`;
  - a single- or double-quoted canonical date string `YYYY-MM-DD`;
  - a single- or double-quoted canonical datetime string
    `YYYY-MM-DD HH:MM:SS`;
  - in row-scalar `SELECT`, a descriptor column whose family is `DATE`,
    `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline `TEXT`;
- complete, valid date values in the current MyLite temporal storage baseline
  range `1000-01-01` through `9999-12-31 23:59:59`;
- output datetime text or `NULL` through existing result APIs.
- result-column metadata through existing public result APIs for the admitted
  scalar and row-scalar shapes described by the runtime observations above.

The row-scalar path lowers to SQLite SQL over stable physical table names and
uses the existing `_mylite_date_interval_second()` scalar callback. SQLite still
performs table scanning, filtering, ordering, and limiting. MyLite does not
materialize the source row set to compute supported `TIMESTAMPADD()` calls.

## Deferred Surface

This slice intentionally does not support:

- units other than `SECOND` and `SQL_TSI_SECOND`, including `MICROSECOND`;
- `SQL_TSI_MICROSECOND`;
- interval expressions, string intervals, decimal/fractional intervals,
  floating-point intervals, boolean intervals, column intervals, parameters,
  user variables, or subqueries;
- relaxed temporal input strings, incomplete dates, zero dates in scalar
  no-source expressions, fractional seconds, numeric temporal coercion, locale
  parsing, named time zones, or `TIMESTAMP` session time-zone conversion;
- `TIME` descriptor arguments or time-only strings;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through.

## Grammar

MyLite admits only this independently authored parser shape:

```lemon
expression(A) ::=
    TIMESTAMPADD(T) LPAREN timestampadd_unit(U) COMMA expression(I)
    COMMA expression(V) RPAREN(R).

timestampadd_unit(A) ::= SECOND(T).
timestampadd_unit(A) ::= SQL_TSI_SECOND(T).
timestampadd_unit(A) ::= timestampdiff_unit(T).  /* parsed for deterministic unsupported units */
```

The runtime accepts only `SECOND` and `SQL_TSI_SECOND`. Other parsed units
return a deterministic unsupported diagnostic for this baseline.

`TIMESTAMPADD` is admitted as an identifier where MyLite admits ordinary
nonreserved identifiers:

```lemon
identifier(A) ::= TIMESTAMPADD(T).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts containing `TIMESTAMPADD()`.
2. Verify the unit is `SECOND` or `SQL_TSI_SECOND`.
3. Decode the interval as the existing signed-integer-or-`NULL` interval-second
   subset.
4. Decode scalar string literals using the current statement SQL mode, including
   `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Resolve row-scalar descriptor columns through MyLite catalog descriptors, not
   SQLite schema text.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. Internal discriminator and literal values are bound
   parameters.
7. Use the existing MyLite-owned SQLite scalar function for table-backed row
   execution.

Evaluation:

1. A `NULL` interval or temporal value returns `NULL`.
2. Date-only inputs are promoted to midnight.
3. Add the signed interval as whole seconds using MyLite-owned Gregorian
   arithmetic.
4. Return `YYYY-MM-DD HH:MM:SS` text for in-range results.
5. Supported in-range expressions produce no warnings.
6. Populate result-column descriptors from MyLite planning evidence: no-source
   and `DUAL` scalar calls remain string-typed; row-backed descriptor
   `DATE`/`DATETIME`/`TIMESTAMP` inputs are exposed as `DATETIME`; row-backed
   descriptor string-family inputs are exposed as strings.

`TIMESTAMPADD()` is pure scalar evaluation. It does not read or mutate catalog
descriptors, descriptor caches, catalog generation, `sqlite_schema_generation`,
physical table definitions, or the `.mylite` preamble.

## Diagnostics

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- wrong function shape: existing parser syntax diagnostics where the grammar
  does not admit the shape;
- unsupported unit:
  `TIMESTAMPADD() supports only SECOND and SQL_TSI_SECOND units`;
- interval is not an integer literal or `NULL`:
  `TIMESTAMPADD() INTERVAL SECOND supports only signed integer literals and NULL`;
- interval integer outside signed 64-bit range:
  `TIMESTAMPADD() INTERVAL SECOND literals must fit the signed 64-bit range`;
- temporal argument is not supported:
  `TIMESTAMPADD() supports only date or datetime string literals and NULL` for
  no-source scalar evaluation, or the row-scalar descriptor-column variant for
  row-backed evaluation;
- temporal string contains embedded NUL:
  `TIMESTAMPADD() date literals do not support NUL bytes`;
- scalar temporal string is not a supported canonical value:
  `TIMESTAMPADD() supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values`;
- arithmetic result outside the supported range:
  `TIMESTAMPADD() result is outside the supported datetime range`;
- unknown row-scalar descriptor columns: existing unknown-column diagnostics;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Tests

Add MySQL-runtime expectation coverage and C runtime/parser tests for:

- successful scalar `SECOND` and `SQL_TSI_SECOND` calls;
- positive, negative, zero, unary-plus, `NULL` interval, and `NULL` temporal
  values;
- date-only input and leap-day rollover;
- `DO`, `FROM DUAL`, column labels, `ROW_COUNT()`, and `@@warning_count`;
- row-scalar table-backed projection over `DATE`, `DATETIME`, `TIMESTAMP`, and
  string descriptor columns, including close/reopen persistence;
- result-column metadata for scalar literal calls and row-backed descriptor
  calls over `DATE`, `DATETIME`, `TIMESTAMP`, and string inputs;
- unknown columns and unsupported `TIME`/integer descriptor columns;
- unsupported units, unsupported interval expressions, string/decimal
  intervals, non-string scalar temporals, invalid scalar temporal strings,
  out-of-range interval literals, and result overflow;
- parser acceptance of whitespace before `(` and identifier use;
- parser rejection of wrong arities and quoted units;
- unchanged file preamble and independent file-backed handles.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-temporal.md` to mark
`TIMESTAMPADD()` as a limited `SECOND`/`SQL_TSI_SECOND` baseline. Do not claim
general temporal arithmetic, broader units, expression intervals, fractional
seconds, predicates, DML assignments, defaults, generated columns, or full
temporal coercion.
