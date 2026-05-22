# Baseline QUARTER Function

## Summary

This phase adds a narrow MySQL-compatible `QUARTER(value)` temporal extraction
slice. It reuses MyLite's descriptor-driven temporal extractor and the
`MYLITE_TEMPORAL_EXTRACT_QUARTER` runtime behavior introduced for
`EXTRACT(QUARTER FROM value)`.

Supported contexts match the existing date-part temporal function baseline:

```sql
SELECT QUARTER(temporal_value)[, ...]
SELECT QUARTER(temporal_value)[, ...] FROM DUAL
DO QUARTER(temporal_value)[, ...]
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

Supported argument forms are:

- `NULL`;
- a single- or double-quoted string literal in a currently supported canonical
  date or datetime shape;
- a descriptor column in a table-backed row-scalar `SELECT`, limited to
  `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, and baseline `TEXT`
  families.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing temporal function specifications:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-extract-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_temporal_extract_functions_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `QUARTER('2008-01-02')`, `QUARTER('2008-04-01')`,
  `QUARTER('2008-07-01')`, and `QUARTER('2008-10-01')` return `1`, `2`, `3`,
  and `4`.
- `QUARTER(datetime)` ignores the time part.
- `QUARTER(NULL)` returns `NULL`.
- `QUARTER('0000-00-00')` and `QUARTER('2005-00-00')` return `0` in empty SQL
  mode; `QUARTER('0000-01-02')` returns `1`; `QUARTER('2001-11-00')` returns
  `4`.
- Invalid date strings return `NULL` with warning `1292` beginning
  `Incorrect datetime value:`.
- Supported successful calls produce no warnings. A scalar `SELECT` makes
  `ROW_COUNT()` return `-1`; a supported `DO` makes it return `0`.
- MySQL accepts numeric, boolean, and hex date coercions such as
  `QUARTER(20080102)`, but those broader coercions are deferred by this
  baseline.
- `QUARTER()` and `QUARTER(value, value)` are syntax errors in MySQL 8.4.9.

## Semantics

`QUARTER(value)` returns:

- `NULL` when `value` is `NULL`;
- `0` when the parsed date month is `0`;
- otherwise `(month + 2) / 3`, producing an integer in the range `1` through
  `4`;
- `NULL` plus warning `1292` when an admitted string or string-family column
  value cannot be parsed as a supported date/datetime input.

Descriptor-backed table projection resolves columns from MyLite catalog
descriptors, not SQLite schema text. Generated SQLite projection SQL uses stable
physical table and column names, quotes generated identifiers, and invokes the
existing `_mylite_temporal_extract` SQLite scalar callback so SQLite still owns
the scan, filtering, ordering, and limiting work.

The function does not mutate catalog rows, descriptor versions, descriptor
caches, catalog generation, or storage metadata.

## Grammar

MyLite adds this parser production:

```lemon
expression(A) ::= QUARTER(T) LPAREN expression(B) RPAREN(R).
```

`QUARTER` remains usable as an unquoted identifier where MyLite admits ordinary
MySQL nonreserved keywords.

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Deferred Surface

This slice intentionally does not support:

- `TIME` descriptor inputs;
- numeric temporal literals, boolean values, bit/hex literals, decimal or float
  values, parameters, variables as arguments, subqueries, or arbitrary
  expressions;
- fractional seconds, relaxed temporal strings, two-digit year coercion,
  compact numeric temporal text, locale or time-zone coercion, or broader
  SQL-mode-sensitive temporal parsing;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through.

## Diagnostics

- Syntax errors use existing parser diagnostics.
- Unsupported argument kinds use the existing temporal-extract unsupported
  diagnostics.
- Unknown row-backed columns reuse existing unknown-column diagnostics.
- Invalid admitted date/datetime values reuse warning `1292`.
- Allocation failure returns `MYLITE_NOMEM`.

## Tests

Tests cover:

- no-source, `FROM DUAL`, and `DO` scalar calls;
- all four quarter values;
- `NULL`, zero-date, partial-zero date, invalid date values, warnings,
  `ROW_COUNT()`, and `@@warning_count`;
- row-backed descriptor columns for `DATE`, `DATETIME`, `TIMESTAMP`, and
  string-family input;
- filtered, ordered, limited row-scalar execution staying in SQLite's scan path;
- unsupported numeric/boolean/hex argument coercions acknowledged as deferred
  against MySQL 8.4.9;
- parser coverage for `QUARTER(expr)` and identifier reuse.
