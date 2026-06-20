# Baseline Grouped Selected Row-Scalar Aggregate Expression Order

## Status

This slice admits grouped `ORDER BY` keys that repeat a selected aggregate
expression whose argument is a supported row-scalar expression:

```sql
SELECT group_column, SUM(integer_column + 1) AS total
FROM source
GROUP BY group_column
ORDER BY SUM(integer_column + 1) [ASC|DESC]
```

The repeated aggregate expression must match one selected aggregate result in
the current grouped aggregate planner. This extends selected aggregate-expression
ordering. Hidden aggregate order keys with row-scalar arguments are covered
separately by
`docs/specs/baseline-hidden-grouped-aggregate-order-keys/specs.md`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline selected aggregate expression order:
  `docs/specs/baseline-grouped-selected-aggregate-expression-order/specs.md`
- Baseline grouped selected bitwise aggregate expression order:
  `docs/specs/baseline-grouped-selected-bitwise-aggregate-expression-order/specs.md`
- Baseline grouped selected statistical aggregate expression order:
  `docs/specs/baseline-grouped-selected-statistical-aggregate-expression-order/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_group_by_multiple_aggregates_expectations.sh`
and
`packages/libmylite/tests/mysql_baseline_statistical_aggregates_expectations.sh`
record MySQL 8.4.9 behavior for repeated selected row-scalar aggregate order
keys. Observed behavior:

- `ORDER BY COUNT(IFNULL(column, 0))` sorts by the selected count result.
- `ORDER BY SUM(column + 1)` places `NULL` aggregate results before non-`NULL`
  aggregate results for ascending order.
- `ORDER BY AVG(column + 1)` sorts by the exact aggregate value and honors
  `DESC` and `LIMIT`.
- `ORDER BY MAX(column + 1)` sorts by the selected maximum value and honors
  `DESC` and `LIMIT`.
- `ORDER BY BIT_OR(column + 1)` sorts by the selected unsigned numeric bitwise
  aggregate result.
- `ORDER BY STDDEV_POP(column + 1)` and `ORDER BY VAR_POP(column + 1)` sort by
  the selected statistical aggregate double value.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...],
       aggregate_function(row_scalar_expression) [AS alias] [, ...]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY aggregate_function(row_scalar_expression) [ASC|DESC]
       [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

The aggregate function is one of the currently executable grouped aggregate
functions that accepts a supported row-scalar argument: `COUNT`, `SUM`, `AVG`,
`MIN`, `MAX`, `BIT_AND`, `BIT_OR`, `BIT_XOR`, `STD`, `STDDEV`, `STDDEV_POP`,
`STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`, or `VARIANCE`. `COUNT(*)`, descriptor
column arguments, aliases, and `DISTINCT` descriptor-column arguments remain
governed by the existing grouped aggregate specs.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
grouped `ORDER BY` aggregate-expression grammar already admits supported
row-scalar aggregate arguments.

## Runtime Semantics

MyLite resolves the `ORDER BY` aggregate expression with the same grouped
aggregate planner used for selected aggregate items. If the planned aggregate
function, distinct flag, and planned row-scalar argument expression match one
selected aggregate, the generated SQLite `ORDER BY` reuses the selected
aggregate order-key builder.

Row-scalar argument matching is structural over MyLite's planned expression
tree, including function/operator discriminators, literals, descriptor-column
identity, source index, conversion metadata, collation metadata, and recursive
arguments. MyLite intentionally does not perform algebraic equivalence. For
example, `SUM(n + 1)` matches `SUM(n + 1)` after planning, but not `SUM(1 + n)`
unless a future planner canonicalizes those expressions.

SQLite continues to perform scanning, grouping, aggregation, ordering, and
limiting. MyLite owns descriptor resolution, selected-expression matching,
aggregate-specific order-key construction, result formatting, diagnostics, and
parameter binding.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- algebraic or semantic equivalence between differently spelled row-scalar
  expressions;
- `GROUP_CONCAT()` aggregate-expression order keys;
- `ANY_VALUE()` aggregate-expression order keys;
- new row-scalar argument functions beyond the already supported row-scalar
  expression envelope;
- broader aggregate argument domains, full grouping, or executable aggregate
  windows.

Unsupported forms continue to return deterministic MyLite diagnostics.
