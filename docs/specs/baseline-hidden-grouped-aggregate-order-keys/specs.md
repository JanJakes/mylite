# Baseline Hidden Grouped Aggregate Order Keys

## Status

This slice admits grouped `ORDER BY` keys that are aggregate expressions but are
not selected as public result columns:

```sql
SELECT group_column [, selected_aggregate ...]
FROM source
GROUP BY group_column
ORDER BY aggregate_function(argument) [ASC|DESC]
```

The aggregate expression must be one of the non-`GROUP_CONCAT()` grouped
aggregate forms already supported by the grouped aggregate planner.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped multiple aggregates:
  `docs/specs/baseline-group-by-multiple-aggregates/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, `GROUP BY` handling:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_group_by_multiple_aggregates_expectations.sh`
records MySQL 8.4.9 behavior for this slice. Observed behavior:

- `SELECT g FROM t GROUP BY g ORDER BY SUM(n)` is accepted and returns only the
  selected `g` column.
- A hidden aggregate order key may be used when other aggregates are selected,
  for example `SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY SUM(n)`.
- Hidden `COUNT(*)`, `SUM(column)`, `AVG(column)`, `ANY_VALUE(column)`, bitwise
  aggregates, statistical aggregates, and supported row-scalar aggregate
  arguments order groups with the same result ordering rules as selected
  repeated aggregate-expression order keys.
- `LIMIT` is applied after hidden aggregate ordering.

## Supported Surface

Supported shape:

```sql
SELECT grouped_projection [, visible_aggregate ...]
FROM descriptor_source
[WHERE supported_predicate]
GROUP BY grouped_key [, grouped_key ...]
[HAVING supported_having]
ORDER BY hidden_aggregate_expression [ASC|DESC]
       [, supported_group_or_aggregate_order_key ...]
[LIMIT supported_limit]
```

The hidden aggregate expression may use:

- `COUNT(*)`;
- `COUNT(column)`;
- `COUNT(DISTINCT column)` for the current descriptor-column subset;
- `MIN(column)`;
- `MAX(column)`;
- `SUM(column)`;
- `AVG(column)`;
- `BIT_AND(column)`;
- `BIT_OR(column)`;
- `BIT_XOR(column)`;
- `ANY_VALUE(column)` in the current descriptor-column grouped envelope;
- `STD()`, `STDDEV()`, `STDDEV_POP()`, `STDDEV_SAMP()`;
- `VAR_POP()`, `VAR_SAMP()`, and `VARIANCE()`;
- supported row-scalar aggregate arguments already admitted by the grouped
  aggregate argument planner.

Hidden aggregate expressions share the current grouped source, grouping,
`WHERE`, `HAVING`, direction, and `LIMIT` envelopes. They count against the
existing sixteen aggregate-planning guard, but they do not appear in public
result metadata or public result rows.

## Non-Goals

This slice does not add:

- hidden `GROUP_CONCAT()` order keys;
- hidden window-function order keys;
- aggregate forms not already supported by grouped aggregate planning;
- broader grouping, source, `HAVING`, collation, binary-string, decimal, or
  exact-result semantics beyond the current grouped aggregate envelopes.

## Runtime Semantics

MyLite plans a hidden aggregate `ORDER BY` key as a grouped aggregate item marked
private to ordering. Generated SQLite SQL renders the aggregate expression only
inside `ORDER BY`, not in the `SELECT` list. SQLite still performs source
scanning, filtering, grouping, aggregate stepping, ordering, and limiting.

Public result-column metadata and row materialization skip hidden order
aggregates. A query such as:

```sql
SELECT g FROM t GROUP BY g ORDER BY SUM(n)
```

therefore produces exactly one public column, `g`, while SQLite orders the
groups by `SUM(n)`.

No SQLite fork hook is required.

## Diagnostics

Unsupported hidden aggregate expressions reuse the existing grouped aggregate
argument diagnostics. Hidden `GROUP_CONCAT()` order keys return a deterministic
unsupported diagnostic. If the hidden aggregate would exceed the current
sixteen-aggregate planning guard, MyLite returns the existing grouped aggregate
limit diagnostic.

## Tests

Coverage lives in the grouped multiple-aggregate and `ANY_VALUE()` MySQL
expectation scripts and the grouped aggregate C runtime tests. Tests cover
hidden `SUM()`, selected result shape with hidden `SUM()`, hidden `COUNT(*)`,
hidden `AVG()`, hidden `ANY_VALUE()`, hidden `BIT_OR()`, hidden
`STDDEV_POP()`, hidden row-scalar `SUM(n + 1)`, `DESC`, tie-break ordering, and
`LIMIT`.
