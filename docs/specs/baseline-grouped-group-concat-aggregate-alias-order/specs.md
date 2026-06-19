# Baseline Grouped GROUP_CONCAT Aggregate Alias Order

## Status

This slice extends grouped aggregate `ORDER BY` so selected `GROUP_CONCAT()`
aggregate aliases may be used as order keys in the existing grouped
`GROUP_CONCAT()` envelope:

```sql
SELECT group_column, GROUP_CONCAT(value ORDER BY order_column SEPARATOR ':') AS names
FROM source
GROUP BY group_column
ORDER BY names [ASC|DESC]
```

The aggregate must be selected and uniquely identified by the alias. This does
not add broader `GROUP_CONCAT()` argument, source, window, or grouped predicate
support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `GROUP_CONCAT()` aggregate:
  `docs/specs/baseline-group-concat-aggregate/specs.md`
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

`packages/libmylite/tests/mysql_baseline_group_concat_aggregate_expectations.sh`
records MySQL 8.4.9 behavior for selected `GROUP_CONCAT()` alias ordering.
Observed behavior:

- `ORDER BY selected_group_concat_alias ASC` sorts by the string value of the
  selected aggregate result, with `NULL` aggregate values first.
- `DESC` reverses non-`NULL` string order and places `NULL` aggregate values
  last.
- Ordering follows the selected nonbinary aggregate text collation for the
  supported ASCII subset, including case-insensitive ordering.
- Alias ordering works when `GROUP_CONCAT()` uses an aggregate-local
  `ORDER BY`, an explicit literal separator, and the existing supported
  row-scalar value argument subset.
- Result row count and warning-count behavior remains the same as the existing
  grouped `GROUP_CONCAT()` path.

## Supported Surface

Supported shape:

```sql
SELECT group_projection,
       GROUP_CONCAT([DISTINCT] value_expression
                    [ORDER BY descriptor_order_column [ASC|DESC]]
                    [SEPARATOR 'literal']) AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY alias [ASC|DESC] [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

The `GROUP_CONCAT()` call remains limited to the current descriptor-backed
grouped envelope:

- one grouped `GROUP_CONCAT()` result;
- one descriptor-backed base-table source for grouped `GROUP_CONCAT()`;
- descriptor value column or already-supported row-scalar value expression;
- optional single descriptor-column aggregate-local order key using the
  existing `GROUP_CONCAT()` order restrictions;
- optional literal separator;
- optional single-value `DISTINCT`.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped `ORDER BY` grammar already admits the
shape.

## Runtime Semantics

The selected `GROUP_CONCAT()` output is already a single physical SQLite result
column. The grouped aggregate executor normally emits internal SQLite SELECT
items without aliases, so selected aggregate-alias ordering is lowered to the
selected output-column ordinal plus the registered MyLite ASCII
`utf8mb4_0900_ai_ci` collation. For example, when `GROUP_CONCAT()` is the
second selected result column, MyLite emits:

```sql
ORDER BY 2 COLLATE "utf8mb4_0900_ai_ci" [ASC|DESC]
```

This intentionally avoids repeating the `GROUP_CONCAT()` aggregate expression.
Repeating the aggregate would be slower and could duplicate
`GROUP_CONCAT()` truncation warnings when `@@group_concat_max_len` cuts the
result. Ordinal-based ordering keeps the selected aggregate as the single
source of result text, `NULL` placement, and warnings while preserving the
user-facing alias semantics.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- `ORDER BY GROUP_CONCAT(...)` or other nonselected aggregate expression order
  keys;
- multiple grouped `GROUP_CONCAT()` results or grouped `GROUP_CONCAT()` mixed
  with other aggregate results;
- broader `GROUP_CONCAT()` value, distinct, separator, or aggregate-local order
  forms;
- grouped `GROUP_CONCAT()` predicates;
- aggregate windows;
- source forms outside the current grouped `GROUP_CONCAT()` envelope.

Unsupported forms continue to return deterministic MyLite diagnostics.
