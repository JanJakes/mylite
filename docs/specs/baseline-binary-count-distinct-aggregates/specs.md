# Baseline Binary COUNT DISTINCT Aggregates

## Status

This slice extends the existing descriptor-column `COUNT(DISTINCT column)`
aggregate paths from integer and nonbinary string descriptors to MySQL binary
string descriptors in the current ungrouped, mixed ungrouped, and grouped
aggregate envelopes.

The feature does not add new syntax. It reuses the existing
`COUNT(DISTINCT column)` grammar and planner surfaces from the ungrouped and
grouped count-distinct specs.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline ungrouped `COUNT(DISTINCT column)`:
  `docs/specs/baseline-count-distinct-column-aggregate/specs.md`
- Baseline string `COUNT(DISTINCT column)`:
  `docs/specs/baseline-string-count-distinct-aggregates/specs.md`
- Baseline binary string types:
  `docs/specs/baseline-binary-string-types/specs.md`
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
`packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh`,
`packages/libmylite/tests/mysql_baseline_group_by_multiple_keys_expectations.sh`,
and
`packages/libmylite/tests/mysql_baseline_joined_aggregate_select_expectations.sh`
record the MySQL probes for this slice. Observed behavior:

- `COUNT(DISTINCT varbinary_column)`, `COUNT(DISTINCT binary_column)`, and
  `COUNT(DISTINCT blob_column)` count unique non-`NULL` byte strings.
- `VARBINARY` and `BLOB` values compare byte-for-byte.
- Fixed `BINARY(N)` values are counted after MySQL storage padding/truncation
  semantics have materialized the stored value.
- Grouped binary count-distinct aggregates apply the same distinct-set
  semantics within each group, including the current multiple-key grouped
  base-table and joined grouped paths.
- Hidden grouped `ORDER BY COUNT(DISTINCT binary_column)` keys follow the same
  grouped aggregate ordering path as integer and nonbinary string
  count-distinct keys.

## Scope

Supported argument columns:

- `BINARY(N)`;
- `VARBINARY(N)`;
- baseline `BLOB` family descriptors.

Supported execution contexts:

- the existing single `SELECT COUNT(DISTINCT column) FROM table [WHERE ...]`
  path;
- the current mixed ungrouped one-table aggregate `SELECT` path;
- the current grouped aggregate paths, including selected grouped `HAVING`,
  selected aggregate-expression ordering, and hidden aggregate-expression
  ordering.

## Runtime Semantics

For binary string descriptor arguments, MyLite lowers the distinct aggregate to
SQLite without a text collation:

```sql
COUNT(DISTINCT "physical_column")
```

SQLite owns source scanning, grouping, and bytewise distinct-set tracking over
the stored blob value. MyLite owns descriptor validation, generated SQL,
diagnostics, result metadata, and public result formatting.

Integer and nonbinary string `COUNT(DISTINCT column)` lowering remains
unchanged. Nonbinary string arguments continue to add MyLite's registered
string-key collation inside the distinct expression.

## Non-Goals

This slice does not add:

- multiple distinct expressions;
- literal or arbitrary expression distinct arguments;
- explicit per-expression `COLLATE` handling inside `COUNT(DISTINCT ...)`;
- wider grouped source forms than the existing grouped count-distinct envelope;
- aggregate windows.

Unsupported argument columns continue to return a deterministic diagnostic:

```text
COUNT(DISTINCT column) supports only integer, string, and binary string descriptor columns
```

## Tests

Coverage is provided by:

- MySQL expectation scripts for ungrouped, mixed, and grouped binary
  count-distinct behavior;
- runtime single-count aggregate tests for `VARBINARY`, fixed `BINARY`, `BLOB`,
  and filtered binary distinct counts;
- mixed aggregate tests for binary count-distinct alongside other aggregate
  results;
- grouped aggregate tests for selected binary count-distinct results, multiple
  grouped keys, selected `HAVING`, joined grouped sources, and hidden grouped
  aggregate order keys.

Run:

```sh
packages/libmylite/tests/mysql_baseline_count_distinct_column_aggregate_expectations.sh
packages/libmylite/tests/mysql_baseline_multi_aggregate_select_expectations.sh
packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh
packages/libmylite/tests/mysql_baseline_group_by_multiple_keys_expectations.sh
packages/libmylite/tests/mysql_baseline_joined_aggregate_select_expectations.sh
ctest --preset dev -R 'libmylite\.runtime\.(count_aggregate|multi_aggregate_select|group_by_single_column_aggregate|joined_aggregate_select)' --output-on-failure
cmake --workflow --preset check
```
