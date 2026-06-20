# Baseline Grouped AVG Aggregate Alias Order

## Status

This slice extends grouped aggregate `ORDER BY` so selected `AVG()` aggregate
aliases may be used as order keys:

```sql
SELECT group_column, AVG(integer_column) AS average_value
FROM source
GROUP BY group_column
ORDER BY average_value [ASC|DESC]
```

It covers selected `AVG([DISTINCT] expr)` aliases in the current
descriptor-backed grouped aggregate envelopes, where the existing grouped AVG
path already lowers the public result through signed-64 `SUM()` plus
`COUNT()`. Repeated selected `ORDER BY AVG(...)` expressions are tracked by
`docs/specs/baseline-grouped-selected-aggregate-expression-order/specs.md`.
This slice does not add hidden `ORDER BY AVG(...)` expressions, broader decimal
widening, aggregate windows, general grouped expression ordering, or new source
forms.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped aggregates:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md`
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

`packages/libmylite/tests/mysql_baseline_group_by_multiple_aggregates_expectations.sh`
and
`packages/libmylite/tests/mysql_baseline_group_by_multiple_keys_expectations.sh`
record MySQL 8.4.9 behavior for `ORDER BY` on selected `AVG()` aliases.
Observed behavior:

- `ORDER BY selected_avg_alias ASC` and `DESC` sort by the exact average value,
  not by the displayed four-fractional-digit text and not by a binary floating
  approximation.
- `NULL` average values from all-`NULL` groups follow normal MySQL aggregate
  ordering placement: first for ascending order and last for descending order.
- Existing `GROUP BY`, `HAVING`, `LIMIT`, warning-count, and row-count behavior
  is unchanged.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...], AVG([DISTINCT] integer_expression) AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_having]
ORDER BY alias [ASC|DESC] [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

The aggregate must be a selected aggregate result and the alias must uniquely
identify that selected aggregate according to the existing grouped aggregate
alias rules. The aggregate argument stays within the current grouped `AVG()`
execution envelope: descriptor integer columns and already-supported row-scalar
integer expressions whose signed-64 `SUM()` intermediate remains in range.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped `ORDER BY` grammar already admits the
shape.

## Runtime Semantics

MyLite continues to lower grouped `AVG()` projection to `SUM(argument)` and
`COUNT(argument)`, then formats the public result as four fractional decimal
digits through the existing grouped-average formatter.

For grouped `ORDER BY` on a selected `AVG()` alias, generated SQL wraps the
same aggregate components in MyLite's internal `_mylite_avg_order_key()`
scalar:

```sql
ORDER BY _mylite_avg_order_key(SUM(argument), COUNT(argument)) [ASC|DESC]
```

The helper returns `NULL` for zero non-`NULL` argument rows. Otherwise it
formats the signed rational `SUM()/COUNT()` value into an internal fixed-width
sortable key with enough fractional digits to preserve ordering across the
current signed-64 numerator and denominator envelope. The public result value
remains the existing four-fractional-digit `AVG()` text.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- hidden `ORDER BY AVG(column)` expressions that are not matched through a
  selected aggregate result;
- wider AVG result precision or decimal widening beyond the current signed-64
  `SUM()` intermediate envelope;
- grouped `GROUP_CONCAT()` alias ordering;
- aggregate windows;
- new source forms outside the current grouped aggregate envelopes.

Unsupported forms continue to return deterministic MyLite diagnostics.
