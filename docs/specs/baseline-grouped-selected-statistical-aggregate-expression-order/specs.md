# Baseline Grouped Selected Statistical Aggregate Expression Order

## Status

This slice admits grouped `ORDER BY` keys that repeat a selected statistical
aggregate expression:

```sql
SELECT group_column, STDDEV_POP(integer_column) AS spread
FROM source
GROUP BY group_column
ORDER BY STDDEV_POP(integer_column) [ASC|DESC]
```

The repeated expression must match one selected statistical aggregate result in
the current grouped aggregate planner. This extends statistical alias ordering.
Hidden statistical aggregate order keys are covered separately by
`docs/specs/baseline-hidden-grouped-aggregate-order-keys/specs.md`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline statistical aggregates:
  `docs/specs/baseline-statistical-aggregates/specs.md`
- Baseline grouped statistical aggregate alias order:
  `docs/specs/baseline-grouped-statistical-aggregate-alias-order/specs.md`
- Baseline selected aggregate expression order:
  `docs/specs/baseline-grouped-selected-aggregate-expression-order/specs.md`
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

`packages/libmylite/tests/mysql_baseline_statistical_aggregates_expectations.sh`
records MySQL 8.4.9 behavior for repeated selected statistical aggregate order
keys. Observed behavior:

- `ORDER BY STDDEV_POP(column)`, `ORDER BY STDDEV_SAMP(column)`,
  `ORDER BY VAR_POP(column)`, and `ORDER BY VAR_SAMP(column)` sort by the
  selected aggregate double value, with `NULL` values first for ascending order.
- `STD()` and `STDDEV()` sort like `STDDEV_POP()`.
- `VARIANCE()` sorts like `VAR_POP()`.
- Descending statistical expression order and `LIMIT` follow normal grouped
  `ORDER BY` behavior.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...],
       statistical_aggregate(integer_descriptor_column) [AS alias] [, ...]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY statistical_aggregate(integer_descriptor_column) [ASC|DESC]
       [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

`statistical_aggregate` is one of `STD`, `STDDEV`, `STDDEV_POP`,
`STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`, or `VARIANCE`. This slice covers
descriptor-column statistical aggregate arguments. The repeated aggregate
expression must match exactly one selected aggregate result after MyLite
descriptor resolution.

MyLite Lemon-syntax grammar snippets:

```lemon
selected_grouped_aggregate_expression ::= IDENTIFIER LPAREN sum_aggregate_argument RPAREN.
/* Builder specialization:
   If IDENTIFIER is STD, STDDEV, STDDEV_POP, STDDEV_SAMP, VAR_POP, VAR_SAMP,
   or VARIANCE, return the matching statistical aggregate node; otherwise
   reject the production as a syntax error. */
```

Selected statistical aggregate-expression ordering for supported row-scalar
aggregate arguments is covered by
`docs/specs/baseline-grouped-selected-row-scalar-aggregate-expression-order/specs.md`.

## Runtime Semantics

MyLite resolves the `ORDER BY` statistical aggregate expression with the same
grouped aggregate planner used for selected aggregate items. If the planned
expression matches one selected statistical aggregate, the generated SQLite
`ORDER BY` reuses the selected aggregate's SQLite aggregate UDF:

```sql
ORDER BY _mylite_stddev_pop(argument) [ASC|DESC]
```

The public result value remains the selected aggregate's double or `NULL` text
result. SQLite continues to perform scanning, grouping, aggregation, ordering,
and limiting. MyLite owns descriptor resolution, selected-expression matching,
statistical function-name specialization, result formatting, diagnostics, and
parameter binding.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- `DISTINCT` statistical aggregate arguments;
- string-to-double coercion warnings or broader noninteger argument domains;
- broader aggregate argument domains, full grouping, or executable aggregate
  windows.

Unsupported forms continue to return deterministic MyLite diagnostics.
