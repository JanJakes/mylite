# Baseline COUNT DISTINCT Row-Scalar Arguments

## Status

This slice extends the current `COUNT()` expression aggregate path from
`COUNT(row_scalar_expression)` to single-expression
`COUNT(DISTINCT row_scalar_expression)` for the documented row-scalar aggregate
argument subset.

The feature does not implement MySQL's multi-expression
`COUNT(DISTINCT expr, expr...)` form. Descriptor-column
`COUNT(DISTINCT column)` remains covered by the existing column-descriptor
count-distinct specs, and grouped row-scalar distinct counts are covered by the
follow-up grouped count-distinct row-scalar spec.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline count aggregate:
  `docs/specs/baseline-count-aggregate/specs.md`
- Baseline universal row-scalar expression contexts:
  `docs/specs/baseline-universal-row-scalar-expression-contexts/specs.md`
- MySQL 8.4 Reference Manual, aggregate function descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_count_distinct_row_scalar_arguments_expectations.sh`
records the MySQL probes for this slice. Observed behavior:

- `COUNT(DISTINCT expr)` counts unique non-`NULL` expression values.
- Empty matched sets and all-`NULL` expression result sets return `0`.
- A non-`NULL` literal distinct argument over a nonempty matched set returns
  `1`; `NULL` literal arguments return `0`.
- Integer arithmetic expressions use their evaluated numeric values for the
  distinct set.
- Nonbinary string expressions use the expression collation, so case and
  trailing-space equivalence follow the active MySQL collation for the covered
  ASCII subset.
- `COUNT(DISTINCT expr)` may appear beside `COUNT(*)`,
  `COUNT(row_scalar_expression)`, and other supported aggregate select items in
  the current mixed aggregate path.

The MySQL manual describes `COUNT(DISTINCT expr [, expr ...])` as counting
different non-`NULL` expression values and returning `0` when no rows match.
MyLite implements only the single-expression subset in this slice.

## Scope

Supported syntax:

```lemon
expression ::= COUNT LPAREN DISTINCT count_row_scalar_aggregate_argument RPAREN.
expression ::= COUNT LPAREN DISTINCT count_literal RPAREN.
```

The parser attaches the ordinary aggregate distinct marker to the existing
`MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION` node for row-scalar arguments and to
the existing `MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION` node for literal
arguments. Existing descriptor-column grammar continues to produce
`MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION`.

Supported execution contexts:

- one-table count-expression aggregate SELECTs over persistent or temporary
  descriptor-backed base tables;
- the current joined count-expression aggregate source envelope;
- optional current `WHERE` and `LIMIT` support for count-expression aggregates;
- mixed select lists that otherwise fit the count-expression aggregate path.

Supported arguments are the same row-scalar aggregate arguments already
accepted by `COUNT(row_scalar_expression)`, plus the existing
`COUNT(literal)` literal subset when preceded by `DISTINCT`.

## Runtime Semantics

MyLite lowers the supported row-scalar form to SQLite as:

```sql
COUNT(DISTINCT <rendered row-scalar expression>)
```

When the row-scalar expression is classified as nonbinary string-valued by the
existing row-scalar collation helper, MyLite appends the registered MySQL-like
string-key collation to the rendered expression before closing the aggregate.
Binary-sensitive `COLLATE` expressions keep their explicit collation lowering.

Literal arguments are lowered as parameters inside `COUNT(DISTINCT ?)`.
SQLite owns scanning and distinct-set aggregation. MyLite owns syntax
classification, row-scalar planning, collation selection, parameter binding,
diagnostics, and result formatting.

## Non-Goals

This slice does not add:

- multiple-expression `COUNT(DISTINCT expr, expr...)`;
- new row-scalar expression families beyond the existing aggregate-argument
  envelope;
- executable distinct expression aggregate windows;
- full Unicode collation parity beyond MyLite's current registered ASCII
  collation approximation.

Unsupported forms continue to return deterministic MyLite diagnostics from the
count-expression aggregate planner or the general aggregate planner.

## Tests

Coverage is provided by:

- MySQL expectation probes for literal, integer arithmetic, string expression,
  filtered, empty, all-`NULL`, joined, and mixed aggregate cases;
- parser tests for `COUNT(DISTINCT row-scalar)` and literal forms;
- runtime count-expression aggregate tests that compare MyLite output with the
  MySQL-observed expectations;
- compatibility documentation updates linking the supported subset.

Run:

```sh
packages/libmylite/tests/mysql_baseline_count_distinct_row_scalar_arguments_expectations.sh
cmake --build --preset dev --target mylite_runtime_count_aggregate_test
ctest --preset dev -R '^libmylite\.runtime\.count_aggregate$' --output-on-failure
cmake --workflow --preset check
```
