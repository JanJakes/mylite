# Baseline String COUNT DISTINCT Aggregates

## Status

This slice extended the existing `COUNT(DISTINCT column)` aggregate paths from
integer descriptor columns to nonbinary string descriptor columns in the current
ungrouped, mixed ungrouped, and grouped aggregate envelopes. Binary string
descriptor columns are covered separately by
`docs/specs/baseline-binary-count-distinct-aggregates/specs.md`.

The feature does not add new syntax. It reuses the existing descriptor-column
`COUNT(DISTINCT column)` grammar and planner surfaces from the ungrouped and
grouped count-distinct specs.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline ungrouped `COUNT(DISTINCT column)`:
  `docs/specs/baseline-count-distinct-column-aggregate/specs.md`
- Baseline grouped `COUNT(DISTINCT column)`:
  `docs/specs/baseline-grouped-count-distinct-aggregate/specs.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation scripts
`packages/libmylite/tests/mysql_baseline_count_distinct_column_aggregate_expectations.sh`,
`packages/libmylite/tests/mysql_baseline_multi_aggregate_select_expectations.sh`,
and
`packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh`
record the MySQL probes for this slice. Observed behavior:

- `COUNT(DISTINCT varchar_column)`, `COUNT(DISTINCT char_column)`, and
  `COUNT(DISTINCT text_column)` count unique non-`NULL` values.
- The default `utf8mb4_0900_ai_ci` comparison folds ASCII case in the covered
  probes, so `alice` and `Alice` count as one distinct value.
- `CHAR` trailing spaces compare as insignificant in the covered probes, so
  `A` and `A   ` count as one distinct value.
- Grouped string count-distinct aggregates apply the same distinct-set
  semantics within each group.
- Selected string count-distinct aggregates can participate in the existing
  selected grouped `HAVING` path.
- Hidden grouped `ORDER BY COUNT(DISTINCT string_column)` keys follow the same
  grouped aggregate ordering path as integer count-distinct keys.

## Scope

Supported argument columns:

- `CHAR`;
- `VARCHAR`;
- baseline `TEXT` family descriptors.

Supported execution contexts:

- the existing single `SELECT COUNT(DISTINCT column) FROM table [WHERE ...]`
  path;
- the current mixed ungrouped one-table aggregate `SELECT` path;
- the current grouped aggregate paths, including selected grouped `HAVING`,
  selected aggregate-expression ordering, and hidden aggregate-expression
  ordering.

## Runtime Semantics

For nonbinary string descriptor arguments, MyLite lowers the distinct aggregate
argument to SQLite with the registered MyLite string-key collation:

```sql
COUNT(DISTINCT "physical_column" COLLATE "utf8mb4_0900_ai_ci")
```

Qualified grouped source columns use the same collation on the qualified
descriptor expression. SQLite owns source scanning, grouping, and distinct-set
tracking; MyLite owns descriptor validation, generated SQL, collation
registration, diagnostics, result metadata, and public result formatting.

Integer `COUNT(DISTINCT column)` lowering remains unchanged.

## Non-Goals

This slice did not add:

- full Unicode collation parity beyond MyLite's current registered limited Unicode
  `utf8mb4_0900_ai_ci` approximation;
- explicit per-expression `COLLATE` handling inside `COUNT(DISTINCT ...)`;
- multiple distinct expressions;
- literal or arbitrary expression distinct arguments;
- wider grouped source forms than the existing grouped count-distinct envelope;
- aggregate windows.

Unsupported argument columns continue to return a deterministic diagnostic
after the later binary-string extension:

```text
COUNT(DISTINCT column) supports only integer, string, and binary string descriptor columns
```

## Tests

Coverage is provided by:

- MySQL expectation scripts for ungrouped, mixed, and grouped string
  count-distinct behavior;
- runtime single-count aggregate tests for `VARCHAR`, `CHAR`, `TEXT`, qualified
  parenthesized string arguments, and filtered string distinct counts;
- mixed aggregate tests for string count-distinct alongside other aggregate
  results;
- grouped aggregate tests for selected string count-distinct results, selected
  `HAVING`, and hidden grouped aggregate order keys;
- varchar lifecycle tests that now assert `COUNT(DISTINCT v)` succeeds.

Run:

```sh
packages/libmylite/tests/mysql_baseline_count_distinct_column_aggregate_expectations.sh
packages/libmylite/tests/mysql_baseline_multi_aggregate_select_expectations.sh
packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh
ctest --preset dev -R 'libmylite\.runtime\.(count_aggregate|multi_aggregate_select|group_by_single_column_aggregate|varchar_type)$' --output-on-failure
cmake --workflow --preset check
```
