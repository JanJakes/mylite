# Baseline Temporal Extract Predicates

## Summary

This phase admits a narrow predicate surface for existing MyLite temporal
extractor functions:

```sql
SELECT ... FROM table_name_or_joined_source
WHERE YEAR(column) = 2008
```

The slice is intentionally limited to descriptor-backed `SELECT` source
envelopes and to numeric temporal extractor results compared with
integer-domain literals. It reuses the existing MyLite row-scalar temporal
extractor planner and registered SQLite scalar function so SQLite still
performs the table scan, joining, filtering, grouping, ordering, and limiting.
MyLite does not materialize rows to evaluate these predicates.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing temporal extractor and predicate designs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-calendar-date-functions/specs.md`
  - `docs/specs/baseline-week-temporal-functions/specs.md`
  - `docs/specs/baseline-string-function-predicates/specs.md`
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
  - comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - function-name parsing and resolution:
    <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_temporal_extract_predicates_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `YEAR()`, `MONTH()`, `QUARTER()`, `DAY()` / `DAYOFMONTH()`, `DAYOFWEEK()`,
  `DAYOFYEAR()`, `WEEK()`, `WEEKDAY()`, `WEEKOFYEAR()`, `YEARWEEK()`,
  `HOUR()`, `MINUTE()`, `SECOND()`, `MICROSECOND()`, `TIME_TO_SEC()`,
  `TO_DAYS()`, and `TO_SECONDS()` return numeric values or `NULL` for the
  already supported argument subset.
- `WHERE extractor(column)` applies normal MySQL truth semantics: nonzero
  numeric values pass, zero values fail, and `NULL` values fail.
- `WHERE NOT extractor(column)` inverts that truth value.
- Comparisons with `=`, `<=>`, `<>` / `!=`, `<`, `<=`, `>`, and `>=` follow
  ordinary numeric comparison behavior. `<=> NULL` and `IS NULL` match rows
  where the extractor result is `NULL`.
- Invalid temporal strings evaluated by the extractor return `NULL`, filter as
  false unless the predicate explicitly matches `NULL`, and append warning
  `1292`, preserving the existing extractor warning text.
- Successful valid in-range predicates produce no warnings.
- MySQL accepts broader shapes such as string comparison operands, arithmetic
  operands, function calls on either side, `IN`, general `BETWEEN`, expression
  ordering, and source envelopes outside MyLite's current descriptor-backed
  `SELECT` joins. This phase defers those shapes.

## Supported SQL

Descriptor-backed `SELECT` using the existing row and joined-source envelopes:

```sql
SELECT select_item[, ...]
FROM table_name_or_joined_source
WHERE temporal_extract_predicate
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted predicate expression is:

```sql
temporal_extract_predicate:
    numeric_temporal_extract_expr
  | NOT numeric_temporal_extract_expr
  | numeric_temporal_extract_expr comparison_operator integer_domain_literal
  | numeric_temporal_extract_expr BETWEEN integer_range_literal AND integer_range_literal
  | numeric_temporal_extract_expr NOT BETWEEN integer_range_literal AND integer_range_literal
  | numeric_temporal_extract_expr IS NULL
  | numeric_temporal_extract_expr IS NOT NULL

comparison_operator:
    = | <=> | <> | != | < | <= | > | >=

integer_domain_literal:
    decimal_integer_literal_with_optional_unary_sign
  | TRUE
  | FALSE
  | NULL

integer_range_literal:
    decimal_integer_literal_with_optional_unary_sign
  | TRUE
  | FALSE
```

`numeric_temporal_extract_expr` is any already supported row-scalar temporal
extractor whose result is numeric for the current descriptor/literal subset:

```sql
YEAR(value)
MONTH(value)
QUARTER(value)
DAY(value)
DAYOFMONTH(value)
DAYOFWEEK(value)
DAYOFYEAR(value)
WEEK(value[, mode])
WEEKDAY(value)
WEEKOFYEAR(value)
YEARWEEK(value[, mode])
HOUR(value)
MINUTE(value)
SECOND(value)
MICROSECOND(value)
TIME_TO_SEC(value)
TO_DAYS(value)
TO_SECONDS(value)
EXTRACT(unit FROM value)
```

The extractor argument support is inherited from the existing row-scalar
temporal extractor implementation. Descriptor columns are resolved through
MyLite catalog descriptors and the selected/default schema policy, not through
SQLite schema text.

## Deferred Surface

This slice intentionally does not support:

- string, decimal, float, hex, bit, parameter, variable, subquery, column, or
  arbitrary expression comparison operands;
- literal-left comparison forms such as `2008 = YEAR(column)`;
- `DATE()`, `TIME()`, `LAST_DAY()`, `DAYNAME()`, `MONTHNAME()`, or other
  text-returning temporal functions in predicates;
- `IN`, general expression `BETWEEN`, `LIKE`, `REGEXP`, arithmetic, scalar
  functions, casts, `CASE`, `IF()`, control-flow, aggregate, window, grouping,
  or having predicates around extractor results;
- source envelopes outside the current descriptor-backed `SELECT` table/join
  support, CTEs, subqueries, DML assignments, defaults, generated columns,
  constraints, expression indexes, or arbitrary SQLite pass-through;
- full MySQL numeric coercion for quoted strings or approximate values.

## Grammar

No new parser tokens are required. MyLite already parses temporal extractor
function calls as expressions and comparison/`IS NULL` predicates. The runtime
admitted shape is:

```lemon
temporal_extract_truth(A) ::= numeric_temporal_extract_expr(B).
temporal_extract_comparison(A) ::=
    numeric_temporal_extract_expr(B) comparison_operator(C) integer_domain_literal(D).
temporal_extract_between(A) ::=
    numeric_temporal_extract_expr(B) BETWEEN(C) integer_range_literal(D)
    AND integer_range_literal(E).
temporal_extract_is_null(A) ::=
    numeric_temporal_extract_expr(B) IS null_operator(C).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect top-level or parenthesized numeric temporal extractor calls in
   `WHERE`, comparison-left, `BETWEEN`, and `IS [NOT] NULL` predicate
   positions.
2. Reuse `plan_row_scalar_temporal_extract_expression()` with the active
   source context so descriptor names, aliases, and unknown-column diagnostics
   stay consistent with other predicate expressions.
3. Convert comparison and range literals with the existing signed-64
   integer-domain parser used by similar function predicate slices. `TRUE`
   maps to `1`, `FALSE` maps to `0`, and comparison `NULL` stays `NULL`.
4. Generate SQLite `WHERE` SQL that calls MyLite's registered
   `_mylite_temporal_extract(...)` function. All literal and discriminator
   values are bound parameters.

Execution:

- Truth predicates pass only non-`NULL`, nonzero numeric extractor results.
- Comparison predicates use SQLite numeric comparison over the MyLite-generated
  integer or `NULL` result. `<=>` maps to SQLite `IS`, matching this limited
  integer/`NULL` domain.
- `BETWEEN` predicates use the same generated extractor expression and integer
  lower/upper bounds, preserving MySQL's inclusive numeric range behavior for
  the admitted literal subset.
- `IS NULL` / `IS NOT NULL` predicates test the extractor result directly.
- Invalid temporal values preserve the existing extractor warning behavior.
- The public result object follows existing `SELECT` conventions.

## Diagnostics

Required diagnostics:

- unsupported text-returning temporal function predicate:
  `temporal extract predicates support only numeric temporal extractor functions`;
- unsupported comparison literal:
  `temporal extract predicates support only integer, boolean, and NULL comparison literals`;
- out-of-range comparison literal:
  `temporal extract predicate comparison literals must fit the signed 64-bit range`;
- unknown descriptor columns use existing MySQL-compatible unknown-column
  diagnostics;
- unsupported temporal argument diagnostics are inherited from the existing
  row-scalar temporal extractor implementation;
- allocation failures use existing MyLite out-of-memory diagnostics.

## Tests

Add runtime tests under `packages/libmylite/tests/` by extending the existing
temporal extractor test binary:

- successful `WHERE YEAR(d) = 2008`, `MONTH(dt)`, `DAYOFMONTH(dt)`,
  `HOUR(tm)`, `MINUTE(dt)`, `SECOND(tm)`, and `QUARTER(d)` comparisons;
- successful `YEAR(d) BETWEEN ...`, `DAYOFMONTH(dt) BETWEEN ...`, and
  `YEAR(d) NOT BETWEEN ...` range predicates;
- truth and `NOT` truth predicates over nonzero, zero, and `NULL` extractor
  results;
- `<=> NULL`, `IS NULL`, and `IS NOT NULL` behavior;
- invalid temporal strings in predicate evaluation, including warning count and
  warnings;
- qualified column and alias resolution;
- joined-source and grouped joined-source filtering;
- order/limit envelope preservation;
- deterministic diagnostics for unknown columns, unsupported text-returning
  extractors, unsupported comparison operands, and out-of-range literals;
- MySQL 8.4.9 expectation script covering the same user-visible behavior.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-temporal.md`, and
`docs/compatibility/functions-temporal.md` to state that only limited numeric
temporal extractor predicates over the current descriptor-backed `SELECT`
source envelopes are supported. Do not overclaim general expression predicates
or full temporal coercion.
