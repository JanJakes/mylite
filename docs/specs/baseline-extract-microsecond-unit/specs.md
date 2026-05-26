# Baseline EXTRACT MICROSECOND Unit

## Summary

This phase extends the existing limited `EXTRACT(unit FROM expr)` runtime to
support `MICROSECOND`. It keeps the existing `EXTRACT()` surface: no-source
scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table row-scalar
projection over descriptor-backed base tables.

The implementation remains MyLite-owned. SQLite continues to execute row
scans, filters, ordering, limits, and storage; MyLite lowers row-backed
projection to its registered `_mylite_temporal_extract` scalar function. No
SQLite fork change is required.

## Sources And Evidence

- Existing MyLite specs:
  - `docs/specs/baseline-extract-function/specs.md`
  - `docs/specs/baseline-microsecond-function/specs.md`
- Official MySQL 8.4 documentation:
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
  - fractional seconds:
    <https://dev.mysql.com/doc/refman/8.4/en/fractional-seconds.html>
- Observed MySQL 8.4.9 behavior captured by
  `packages/libmylite/tests/mysql_baseline_extract_function_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code.

## MySQL 8.4.9 Runtime Observations

- `EXTRACT(MICROSECOND FROM '12:00:00.123456')` returns `123456`.
- Short fractions are padded; `.1` returns `100000`.
- More than six fractional digits are rounded to the six-digit microsecond
  part; `.1234567` returns `123457`, and `.9999995` yields part `0`.
- Datetime strings with fractional seconds return the fractional part.
- Time or datetime strings without fractional seconds return `0`.
- `NULL` input returns `NULL`.
- Date-only strings return `0` and warning `1292` with text beginning
  `Truncated incorrect time value:`.
- Invalid non-`NULL` strings, including datetime-shaped strings whose time
  part is outside `00:00:00` through `23:59:59`, return `NULL` with the same
  warning shape.
- Negative time strings preserve sign for `EXTRACT(MICROSECOND ...)`:
  `EXTRACT(MICROSECOND FROM '-13:29:17.000006')` returns `-6`. This differs
  from `MICROSECOND('-13:29:17.000006')`, which returns `6`.
- Stored `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` descriptor values in
  MyLite's current zero-fractional storage envelope return `0` for
  non-`NULL` values.

## Supported SQL

```sql
SELECT EXTRACT(MICROSECOND FROM temporal_value)[, ...]
SELECT EXTRACT(MICROSECOND FROM temporal_value)[, ...] FROM DUAL
DO EXTRACT(MICROSECOND FROM temporal_value)[, ...]
```

Single-table row-backed projection is supported through the existing limited
`SELECT` shape:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

Admitted `temporal_value` forms:

- `NULL`;
- quoted canonical time strings with optional fractional seconds;
- quoted canonical datetime strings with optional fractional seconds;
- quoted date-only strings, returning `0` with warning `1292`;
- descriptor `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, and
  baseline `TEXT` family columns.

## Deferred Surface

This phase does not add `WEEK`, microsecond composite units
(`DAY_MICROSECOND`, `HOUR_MICROSECOND`, `MINUTE_MICROSECOND`,
`SECOND_MICROSECOND`), numeric temporal coercion, parameters, variables,
subqueries, arbitrary expressions, predicates over `EXTRACT()` results, DML
assignment expressions, generated columns, defaults, indexes, or broader
SQL-mode-sensitive temporal parsing.

## Grammar

No parser grammar change is required. Existing MyLite grammar already admits:

```lemon
expression(A) ::= EXTRACT(T) LPAREN extract_unit(U) FROM expression(V) RPAREN(R).
extract_unit(A) ::= MICROSECOND(T).
```

This phase changes runtime analysis of the parsed `MICROSECOND` unit from a
deterministic unsupported diagnostic to a supported signed microsecond
extraction kind.

## Runtime And Ownership Boundaries

- Public API: no ABI change.
- Parser/AST: no generated grammar expansion; existing parsed
  `EXTRACT(MICROSECOND FROM expr)` nodes are reused.
- Analyzer/planner: maps the `MICROSECOND` unit to a distinct signed
  microsecond extract kind so `EXTRACT()` sign behavior stays separate from
  `MICROSECOND()`.
- Catalog: descriptor resolution stays authoritative and does not consult
  SQLite metadata.
- Runtime: reuses MyLite's fractional-second parser and warning machinery.
- SQLite: row-backed projection continues through the registered scalar
  callback; there is no SQLite fork patch.

## Diagnostics And Warnings

- Unsupported argument kinds reuse existing `EXTRACT()` temporal argument
  diagnostics.
- Unsupported descriptor families reuse existing temporal extractor
  diagnostics.
- Invalid strings return `NULL` and append warning `1292`.
- Date-only strings return `0` and append warning `1292`.
- Unsupported `WEEK` and microsecond composite units remain unsupported.
- Allocation failures propagate through existing out-of-memory diagnostics.

## Tests

Coverage is added to the existing EXTRACT expectation and runtime tests:

- MySQL 8.4.9 expectation probes for positive, negative, rounded, datetime,
  date-only, invalid, and table-backed `MICROSECOND` extraction.
- MyLite no-source, `DUAL`/`DO`, row-backed descriptor, warning, and reopen
  coverage in `runtime_temporal_extract_functions_test.c`.
- Continued deterministic rejection for `WEEK` and microsecond composite units.
