# Baseline Grouped Statistical Aggregate Alias Order

## Status

This slice extends grouped aggregate `ORDER BY` so selected statistical
aggregate aliases may be used as order keys in the existing descriptor-backed
grouped aggregate envelopes:

```sql
SELECT group_column, STDDEV_POP(integer_expression) AS spread
FROM source
GROUP BY group_column
ORDER BY spread [ASC|DESC]
```

The aggregate must be selected and uniquely identified by the alias. This does
not add broader grouping, window, `DISTINCT`, string-coercion, or source-form
support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline statistical aggregates:
  `docs/specs/baseline-statistical-aggregates/specs.md`
- Baseline multiple grouped aggregates:
  `docs/specs/baseline-group-by-multiple-aggregates/specs.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_statistical_aggregates_expectations.sh`
records MySQL 8.4.9 behavior for selected statistical aggregate alias
ordering. Observed behavior:

- `ORDER BY selected_stddev_pop_alias ASC` sorts by the aggregate double value,
  with `NULL` aggregate results first.
- `DESC` places the largest non-`NULL` double values first and `NULL` aggregate
  results last.
- `STDDEV_SAMP()` and `VAR_SAMP()` aliases order by their sample aggregate
  results; groups with fewer than two non-`NULL` inputs produce `NULL`.
- `VAR_POP()` / `VARIANCE()` aliases order by population variance results.
- Supported row-scalar statistical aggregate arguments order by the selected
  aggregate result.
- Result row count and warning-count behavior remains the same as the existing
  grouped statistical aggregate path.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...],
       statistical_aggregate(integer_expression) AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY alias [ASC|DESC] [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

`statistical_aggregate` is one of `STD`, `STDDEV`, `STDDEV_POP`,
`STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`, or `VARIANCE`. The argument stays within
the current grouped statistical aggregate execution envelope: integer
descriptor columns and already-supported row-scalar integer expressions.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped `ORDER BY` grammar already admits the
shape.

## Runtime Semantics

MyLite continues to lower grouped statistical aggregate projection to the
registered MyLite aggregate UDFs:

- `STD`, `STDDEV`, `STDDEV_POP` -> `_mylite_stddev_pop`
- `STDDEV_SAMP` -> `_mylite_stddev_samp`
- `VAR_POP`, `VARIANCE` -> `_mylite_var_pop`
- `VAR_SAMP` -> `_mylite_var_samp`

For grouped `ORDER BY` on a selected statistical aggregate alias, generated SQL
reuses the same aggregate expression in the SQLite `ORDER BY` clause:

```sql
ORDER BY _mylite_stddev_pop(argument) [ASC|DESC]
```

The helper result is `NULL` for empty/all-`NULL` population aggregates and for
sample aggregates with fewer than two non-`NULL` arguments. Otherwise SQLite
orders the registered aggregate's double result. This is sufficient for the
current MyLite statistical envelope and requires no SQLite fork hook.

## Non-Goals

This slice does not add:

- repeated statistical aggregate expressions, which are tracked by
  `docs/specs/baseline-grouped-selected-statistical-aggregate-expression-order/specs.md`
  for selected descriptor-column statistical aggregate expressions;
- `DISTINCT` statistical aggregate arguments;
- executable aggregate windows;
- string-to-double coercion warnings or broader noninteger argument domains;
- broader source forms outside the current grouped aggregate envelopes;
- full MySQL grouping semantics outside the documented grouped planner subset.

Unsupported forms continue to return deterministic MyLite diagnostics.
