# Baseline Grouped Selected Aggregate Expression Order

## Status

This slice admits grouped `ORDER BY` keys that repeat a selected aggregate
expression:

```sql
SELECT group_column, SUM(integer_column) AS total
FROM source
GROUP BY group_column
ORDER BY SUM(integer_column) [ASC|DESC]
```

The order expression must match one selected aggregate result in the current
grouped aggregate planner. This is separate from selected alias ordering and
does not add hidden aggregate order keys.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline multiple grouped aggregates:
  `docs/specs/baseline-group-by-multiple-aggregates/specs.md`
- Baseline grouped core aggregate alias order:
  `docs/specs/baseline-grouped-core-aggregate-alias-order/specs.md`
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
records MySQL 8.4.9 behavior for aggregate-expression order keys. Observed
behavior:

- `ORDER BY COUNT(*)`, `ORDER BY COUNT(column)`, and
  `ORDER BY COUNT(DISTINCT column)` sort by the selected integer aggregate
  result.
- `ORDER BY SUM(column) ASC` places `NULL` aggregate results before non-`NULL`
  aggregate results, while descending order places them last.
- `ORDER BY MIN(column)` and `ORDER BY MAX(column)` sort by the selected
  aggregate value.
- `ORDER BY AVG(column)` sorts by the aggregate numeric value, not by the
  formatted result label.
- Combining an aggregate-expression order key with a supported grouped-column
  tiebreaker follows normal `ORDER BY` list order.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...], aggregate_expression [AS alias] [, ...]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY aggregate_expression [ASC|DESC] [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

The repeated `aggregate_expression` must match exactly one selected aggregate
result after MyLite descriptor resolution. Supported expressions are the
current grouped descriptor-column forms:

- `COUNT(*)`
- `COUNT(integer_descriptor_column)`
- `COUNT(DISTINCT integer_descriptor_column)`
- `MIN(integer_descriptor_column)`
- `MAX(integer_descriptor_column)`
- `SUM(integer_descriptor_column)`
- `AVG(integer_descriptor_column)`

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
grouped `ORDER BY` grammar already admits the aggregate-expression keys.

## Runtime Semantics

MyLite resolves the `ORDER BY` aggregate expression with the same grouped
aggregate planner used for selected aggregate items. If the planned expression
matches one selected aggregate, the generated SQLite `ORDER BY` reuses the
selected aggregate's existing order-key builder:

- ordinary aggregate order keys repeat the aggregate expression;
- `AVG()` uses MyLite's exact signed-rational `_mylite_avg_order_key(...)`;
- selected grouped-column tiebreakers still use descriptor-column ordering.

SQLite continues to perform scanning, grouping, aggregation, ordering, and
limiting. MyLite owns descriptor resolution, selected-expression matching,
unsupported diagnostics, result formatting, and parameter binding.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- hidden aggregate order keys that do not match a selected aggregate result,
  such as `SELECT g FROM t GROUP BY g ORDER BY SUM(n)`;
- row-scalar aggregate argument matching such as selected `SUM(n + 1)` ordered
  by `SUM(n + 1)`;
- `GROUP_CONCAT()` aggregate-expression order keys;
- bitwise or statistical aggregate-expression order keys, which are still
  parser gaps for `ORDER BY` expressions and remain covered through selected
  alias ordering;
- duplicate selected aggregate expression ambiguity handling beyond a
  deterministic unsupported diagnostic;
- broader aggregate argument domains, hidden aggregate projection, full
  grouping, or executable aggregate windows.

Unsupported forms continue to return deterministic MyLite diagnostics.
