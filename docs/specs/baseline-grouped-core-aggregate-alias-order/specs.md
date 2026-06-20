# Baseline Grouped Core Aggregate Alias Order

## Status

This slice documents the grouped aggregate `ORDER BY` surface where selected
core aggregate aliases are used as order keys in the existing grouped aggregate
envelopes:

```sql
SELECT group_column, SUM(integer_expression) AS total
FROM source
GROUP BY group_column
ORDER BY total [ASC|DESC]
```

The selected aggregate alias must uniquely identify one selected aggregate
result. This slice covers `COUNT(*)`, `COUNT(column)`,
`COUNT(DISTINCT column)`, `MIN(expr)`, `MAX(expr)`, and `SUM(expr)` in their
already-supported grouped execution envelopes. It does not add nonselected
aggregate expression ordering, broader aggregate argument domains, or new
grouped source forms.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline multiple grouped aggregates:
  `docs/specs/baseline-group-by-multiple-aggregates/specs.md`
- Baseline multiple grouped keys:
  `docs/specs/baseline-group-by-multiple-keys/specs.md`
- Baseline grouped `COUNT(DISTINCT column)`:
  `docs/specs/baseline-grouped-count-distinct-aggregate/specs.md`
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
record MySQL 8.4.9 behavior for selected core aggregate alias ordering.
Observed behavior:

- `ORDER BY selected_count_alias DESC` sorts by the integer count result.
- `ORDER BY selected_sum_alias ASC` sorts `NULL` aggregate results before
  non-`NULL` aggregate results; `DESC` places `NULL` results after non-`NULL`
  aggregate results.
- `ORDER BY selected_min_alias ASC` and `ORDER BY selected_max_alias DESC`
  sort by the selected aggregate value, not by result labels.
- `ORDER BY selected_count_distinct_alias DESC` works in the verified
  multiple-key grouped envelope.
- Existing grouped `HAVING`, `LIMIT`, warning-count, and row-count behavior is
  unchanged.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...], core_aggregate AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY alias [ASC|DESC] [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

`core_aggregate` is one of:

- `COUNT(*)`
- `COUNT(integer_descriptor_column)`
- `COUNT(DISTINCT integer_descriptor_column)`
- `MIN(supported_descriptor_or_row_scalar_expression)`
- `MAX(supported_descriptor_or_row_scalar_expression)`
- `SUM(supported_integer_descriptor_or_row_scalar_expression)`

The aggregate argument stays within each function's current grouped execution
envelope. The alias must be selected and unique according to the current
grouped aggregate alias-resolution rules.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped `ORDER BY` grammar already admits the
shape.

## Runtime Semantics

MyLite lowers the selected aggregate alias order key to the same aggregate
expression that backs the selected result:

```sql
ORDER BY COUNT(*) [ASC|DESC]
ORDER BY COUNT(DISTINCT "physical_column") [ASC|DESC]
ORDER BY MIN(argument) [ASC|DESC]
ORDER BY MAX(argument) [ASC|DESC]
ORDER BY SUM(argument) [ASC|DESC]
```

For the current integer and `NULL` descriptor-backed envelopes, SQLite's native
aggregate ordering matches the verified MySQL subset. MyLite still owns
descriptor resolution, alias uniqueness checks, result formatting, diagnostics,
and parameter binding.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- `ORDER BY COUNT(*)`, `ORDER BY SUM(column)`, or other nonselected aggregate
  expression order keys;
- duplicate selected aggregate alias ambiguity handling beyond the current
  deterministic unsupported diagnostic;
- broader `COUNT(DISTINCT ...)` forms such as multiple expressions, literals,
  or arbitrary row-scalar expressions;
- broader string, binary, decimal, approximate, JSON, enum, set, or temporal
  aggregate ordering semantics beyond the current grouped aggregate envelope;
- executable aggregate windows;
- full MySQL grouping semantics outside the documented grouped planner subset.

Unsupported forms continue to return deterministic MyLite diagnostics.
