# Baseline Grouped COUNT DISTINCT Row-Scalar Arguments

## Status

This slice extends the grouped aggregate path from descriptor-column
`COUNT(DISTINCT column)` and non-distinct grouped `COUNT(row_scalar_expression)`
to grouped single-expression `COUNT(DISTINCT row_scalar_expression)` plus
grouped `COUNT(DISTINCT literal)` for the existing aggregate row-scalar and
literal argument subsets.

The feature remains intentionally narrower than MySQL's full
`COUNT(DISTINCT expr [, expr...])` surface. It does not add multiple distinct
arguments, grouped aggregate windows, or new row-scalar expression families.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped `COUNT(DISTINCT column)`:
  `docs/specs/baseline-grouped-count-distinct-aggregate/specs.md`
- Baseline ungrouped `COUNT(DISTINCT row_scalar_expression)`:
  `docs/specs/baseline-count-distinct-row-scalar-arguments/specs.md`
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
`packages/libmylite/tests/mysql_baseline_grouped_count_distinct_row_scalar_arguments_expectations.sh`
records the MySQL probes for this slice. Observed behavior:

- `COUNT(DISTINCT literal)` is evaluated per group. Non-`NULL` literals return
  `1` for each nonempty group; `NULL` literals return `0`.
- `COUNT(DISTINCT row_scalar_expression)` counts unique non-`NULL` evaluated
  expression values per group.
- Arithmetic, `IFNULL()`, `NULLIF()`, `CONCAT()`, `LOWER()`, and `CAST(... AS
  CHAR)` expressions follow the same per-row value and `NULL` behavior as the
  ungrouped count-expression path, then distinct aggregation is applied inside
  each group.
- Nonbinary string row-scalar expressions use the expression collation, so the
  covered ASCII subset folds case and ignores insignificant `CHAR` trailing
  spaces like the corresponding grouped descriptor-column path.
- Selected grouped aliases and repeated selected aggregate expressions may be
  used in the current grouped `HAVING` and aggregate-expression `ORDER BY`
  envelopes.

## Scope

Supported selected aggregate forms:

```lemon
selected_grouped_aggregate_expression ::= COUNT LPAREN DISTINCT count_literal RPAREN.
selected_grouped_aggregate_expression ::= COUNT LPAREN DISTINCT count_row_scalar_aggregate_argument RPAREN.
```

The supported source, grouping, `WHERE`, `HAVING`, `ORDER BY`, and `LIMIT`
envelopes are inherited from the current grouped aggregate planner:

- one descriptor-backed base table grouped by one to four supported descriptor
  keys;
- the current selected grouped aggregate result list;
- current grouped `HAVING` predicates over the selected aggregate alias or the
  matching selected aggregate expression;
- current grouped aggregate-expression ordering and limiting.

The selected expression may appear beside other aggregate results when the
select list otherwise fits the grouped aggregate envelope.

## Runtime Semantics

MyLite lowers the supported forms to SQLite grouped aggregates:

```sql
COUNT(DISTINCT <literal-or-rendered-row-scalar-expression>)
```

Literal arguments are rendered through the existing row-scalar parameter
binding path. Row-scalar arguments are planned with the existing aggregate
argument planner. When the planned row-scalar expression is classified as
nonbinary string-valued, MyLite appends the registered MySQL-like string-key
collation to the rendered expression before closing the aggregate call.

SQLite owns grouped scanning, distinct aggregation, `HAVING`, ordering, and
limiting. MyLite owns syntax classification, descriptor and expression
planning, collation selection, parameter binding, diagnostics, and result
metadata.

This implementation uses MyLite wrapper/translation logic over public SQLite
APIs. It does not require a SQLite fork hook.

## Non-Goals

This slice does not add:

- multiple-expression `COUNT(DISTINCT expr, expr...)`;
- row-scalar expression families outside the current aggregate-argument
  subset;
- grouped distinct row-scalar forms outside the existing grouped aggregate
  source envelope;
- aggregate expressions that appear only in `HAVING`;
- executable aggregate windows;
- full Unicode collation parity beyond MyLite's current registered ASCII
  collation approximation.

Unsupported forms continue to return deterministic MyLite diagnostics from the
grouped aggregate planner or the general aggregate planner.

## Tests

Coverage is provided by:

- MySQL expectation probes for grouped distinct literals, arithmetic
  expressions, control-flow expressions, string expressions, selected aggregate
  aliases and repeated aggregate expressions in `HAVING`, and repeated
  aggregate expressions in `ORDER BY`;
- runtime grouped aggregate tests over the existing numeric and string grouped
  fixtures;
- compatibility documentation updates linking the supported subset.

Run:

```sh
packages/libmylite/tests/mysql_baseline_grouped_count_distinct_row_scalar_arguments_expectations.sh
cmake --build --preset dev --target mylite_runtime_group_by_single_column_aggregate_test
ctest --preset dev -R '^libmylite\.runtime\.group_by_single_column_aggregate$' --output-on-failure
cmake --workflow --preset check
```
