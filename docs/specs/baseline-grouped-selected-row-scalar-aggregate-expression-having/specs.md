# Baseline Grouped Selected Row-Scalar Aggregate Expression HAVING

## Status

This slice extends the documented grouped `HAVING` baseline from selected
descriptor-column aggregate expressions to selected aggregate expressions whose
argument is a supported row-scalar expression.

The feature is intentionally narrow. It does not add aggregate expressions
that appear only in `HAVING`, boolean composition, broader `HAVING` expression
evaluation, or new row-scalar expression families. Bitwise aggregate result
predicates are covered by
`docs/specs/baseline-grouped-bitwise-aggregate-having/specs.md`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- Baseline grouped selected row-scalar aggregate expression ordering:
  `docs/specs/baseline-grouped-selected-row-scalar-aggregate-expression-order/specs.md`
- Baseline universal row-scalar expression contexts:
  `docs/specs/baseline-universal-row-scalar-expression-contexts/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax and `HAVING` evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, aggregate function descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_grouped_selected_row_scalar_aggregate_expression_having_expectations.sh`
records the MySQL probes for this slice. Observed behavior:

- A grouped `HAVING` predicate may repeat a selected aggregate expression whose
  argument is a row-scalar expression.
- The repeated `HAVING` aggregate expression is evaluated after grouping and
  filters groups before `ORDER BY` and `LIMIT`.
- `COUNT(row_scalar_expression)` ignores `NULL` expression results.
- `SUM()`, `AVG()`, `MIN()`, and `MAX()` over supported row-scalar arguments
  follow their normal grouped aggregate semantics, including `NULL` handling.

## Scope

Supported statement shape:

```sql
SELECT group_projection [, ...],
       aggregate_function(row_scalar_expression) [AS alias] [, ...]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
HAVING aggregate_function(row_scalar_expression) comparison_operator integer_literal
[ORDER BY supported_grouped_order_key ...]
[LIMIT supported_limit]
```

The repeated aggregate expression in `HAVING` must match one selected
aggregate expression. It may be parenthesized where the existing grouped
`HAVING` grammar allows a parenthesized operand.

The supported aggregate functions are the non-bitwise grouped aggregate
functions that already accept row-scalar arguments in the current grouped
aggregate envelope:

- `COUNT(row_scalar_expression)`;
- `SUM(row_scalar_expression)`;
- `AVG(row_scalar_expression)`;
- `MIN(row_scalar_expression)`;
- `MAX(row_scalar_expression)`.

`COUNT(DISTINCT row_scalar_expression)` is covered by the grouped
count-distinct row-scalar companion slice.

Supported comparison operators and integer-literal conversion are inherited
from `baseline-having-grouped-aggregate`.

## Runtime Semantics

MyLite resolves the repeated `HAVING` aggregate expression by planning it as a
grouped aggregate item and matching it against selected aggregate items. The
selected aggregate item owns the SQL rendering, parameter binding, result type,
and MySQL-compatible aggregate formatting.

SQLite owns the grouped scan, aggregate evaluation, `HAVING` filtering,
ordering, and limiting. MyLite owns syntax classification, row-scalar argument
planning, descriptor resolution, selected-aggregate matching, diagnostics, and
result metadata.

This implementation uses MyLite wrapper/translation logic over public SQLite
APIs. It does not require a SQLite fork hook.

## Non-Goals

This slice does not add:

- aggregate expressions that appear only in `HAVING`;
- row-scalar expression families outside the current aggregate-argument
  subset;
- bitwise aggregate result predicates outside
  `docs/specs/baseline-grouped-bitwise-aggregate-having/specs.md`;
- `GROUP_CONCAT()` result predicates, which are covered by dedicated
  `GROUP_CONCAT()` HAVING slices;
- boolean composition such as `AND`, `OR`, `XOR`, or `NOT`;
- bare truth tests such as `HAVING SUM(expr)`;
- non-integer comparison literals;
- executable aggregate windows;
- broader source or grouping forms than the current grouped aggregate planner.

Unsupported forms continue to return deterministic MyLite diagnostics from the
grouped aggregate planner.

## Tests

Coverage is provided by:

- MySQL expectation probes for selected `COUNT()`, `SUM()`, `AVG()`, `MIN()`,
  and `MAX()` row-scalar aggregate expressions repeated in grouped `HAVING`;
- runtime grouped aggregate tests over the existing numeric grouped fixture;
- compatibility documentation updates linking the supported subset.

Run:

```sh
packages/libmylite/tests/mysql_baseline_grouped_selected_row_scalar_aggregate_expression_having_expectations.sh
cmake --build --preset dev --target mylite_runtime_group_by_single_column_aggregate_test
ctest --preset dev -R '^libmylite\.runtime\.group_by_single_column_aggregate$' --output-on-failure
cmake --workflow --preset check
```
