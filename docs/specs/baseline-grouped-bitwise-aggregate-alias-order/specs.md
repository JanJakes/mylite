# Baseline Grouped Bitwise Aggregate Alias Order

## Status

This slice extends grouped aggregate `ORDER BY` so selected bitwise aggregate
aliases may be used as order keys:

```sql
SELECT group_column, BIT_OR(integer_column) AS bits
FROM source
GROUP BY group_column
ORDER BY bits [ASC|DESC]
```

It covers selected `BIT_AND()`, `BIT_OR()`, and `BIT_XOR()` aliases in the
current descriptor-backed grouped aggregate envelopes. It does not add bitwise
aggregate result predicates, bitwise aggregate expressions that are not
selected, binary-string bitwise aggregate semantics, aggregate windows, or
general grouped expression ordering.

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
records MySQL 8.4.9 behavior for `ORDER BY` on selected `BIT_AND()`,
`BIT_OR()`, and `BIT_XOR()` aliases. Observed behavior:

- `ORDER BY selected_bitwise_alias ASC` sorts by the unsigned numeric bitwise
  aggregate value, not by the displayed decimal text.
- The selected bitwise aggregate result is still displayed as unsigned decimal
  text.
- Existing `GROUP BY`, `HAVING`, `LIMIT`, and row-count behavior is unchanged.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...], bitwise_aggregate(integer_column) AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_having]
ORDER BY alias [ASC|DESC] [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

`bitwise_aggregate` is `BIT_AND`, `BIT_OR`, or `BIT_XOR`. The aggregate must be
a selected aggregate result and the alias must uniquely identify that selected
aggregate according to the existing grouped aggregate alias rules.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped `ORDER BY` grammar already admits the
shape.

## Runtime Semantics

MyLite continues to lower bitwise grouped aggregates to the internal SQLite
aggregate functions that produce unsigned decimal text. For grouped `ORDER BY`
on selected bitwise aggregate aliases, generated SQL wraps the bitwise aggregate
expression in MyLite's internal `_mylite_uint64_decimal_order_key()` scalar. The
wrapper validates the decimal text and returns a fixed-width zero-padded
decimal key, which gives SQLite lexicographic ordering equivalent to unsigned
64-bit numeric ordering.

The public result value remains the original unsigned decimal text. The helper
is an internal SQL function registered with the SQLite connection; it does not
change MyLite's public C ABI or file format.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- `ORDER BY BIT_OR(column)` or other aggregate expressions that are not matched
  through a selected alias;
- grouped `HAVING` predicates on bitwise aggregate results;
- binary-string bitwise aggregate evaluation;
- `DISTINCT` bitwise aggregate arguments;
- `GROUP_CONCAT()` or exact `AVG()` aggregate-alias ordering;
- aggregate windows;
- new source forms outside the current grouped aggregate envelopes.

Unsupported forms continue to return deterministic MyLite diagnostics.
