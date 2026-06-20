# Baseline Grouped COUNT DISTINCT Aggregate

## Status

This slice extends the existing descriptor-backed grouped aggregate path with
`COUNT(DISTINCT column)` over integer, nonbinary string, and binary string
descriptor columns.
It builds on the baseline `GROUP BY`, `HAVING`, and ungrouped
`COUNT(DISTINCT column)` features.
Grouped single-expression literal and row-scalar distinct-count arguments are
covered by
`docs/specs/baseline-grouped-count-distinct-row-scalar-arguments/specs.md`.

The feature is intentionally narrow. It supports the current base-table grouped
aggregate path, including the current multiple-key grouped base-table path,
and the current two-source joined grouped aggregate path with one distinct
descriptor-column argument. It does not implement MySQL's full
`COUNT(DISTINCT expr[, expr...])` surface, aggregate windows, joined
multiple-key grouped sources, or arbitrary grouped expressions beyond the
companion grouped row-scalar slice.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped aggregate:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md`
- Baseline grouped `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- Baseline ungrouped `COUNT(DISTINCT column)`:
  `docs/specs/baseline-count-distinct-column-aggregate/specs.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `GROUP BY` handling:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `COUNT(DISTINCT column)` in a grouped select returns the number of unique
  non-`NULL` argument values per group.
- Duplicate values inside a group are counted once.
- Groups where every argument value is `NULL` return `0`.
- Multiple-key base-table grouping applies the same distinct-count semantics
  per grouped key tuple.
- Nonbinary string descriptor arguments apply string distinct-count semantics
  per group. The covered `VARCHAR`, `CHAR`, and `TEXT` probes use MySQL's
  default collation behavior, including ASCII case folding and insignificant
  trailing spaces for `CHAR`.
- Binary string descriptor arguments apply bytewise distinct-count semantics
  per group.
- The aggregate may be selected with an alias and filtered by that alias in
  `HAVING`.
- A matching selected aggregate expression such as
  `HAVING COUNT(DISTINCT i) > 1` filters groups after aggregation.
- In the multiple-key base-table grouped path, selected `COUNT(DISTINCT column)`
  aliases may be used as grouped `ORDER BY` keys, following the existing
  count-like aggregate alias ordering path.
- Successful grouped selects set `ROW_COUNT()` to `-1` and leave warning count
  `0`, matching the existing grouped aggregate contract.

## Scope

Supported shape:

```sql
SELECT group_column [AS alias], COUNT(DISTINCT aggregate_column) [AS alias]
FROM source
[WHERE predicate]
GROUP BY group_column
[HAVING selected_count_distinct_predicate]
[ORDER BY supported_group_or_selected_alias_order]
[LIMIT supported_limit]
```

`source` is either one descriptor-backed persistent base table or the current
two-source joined grouped source envelope. A base-table source may use the
current one-to-four descriptor-key grouping subset. The joined source envelope
keeps the current joined grouped-key limits. `group_column`, `WHERE`,
`ORDER BY`, and `LIMIT` inherit the current grouped aggregate envelope.
`aggregate_column` must resolve to one integer, nonbinary string, or binary
string descriptor column from the grouped source. Source-qualified and
parenthesized descriptor-column forms admitted by the existing parser are
supported when they resolve to that descriptor column.

Supported `HAVING` operands:

```sql
selected_count_distinct_alias
COUNT(DISTINCT aggregate_column)
COUNT(DISTINCT(aggregate_column))
```

They may be compared with the existing grouped aggregate `HAVING` integer and
boolean literal predicate subset, or tested with `IS NULL` / `IS NOT NULL`.

MyLite Lemon-syntax grammar snippets:

```lemon
selected_grouped_aggregate_expression ::= COUNT LPAREN DISTINCT qualified_identifier RPAREN.
selected_grouped_aggregate_expression ::= COUNT LPAREN DISTINCT LPAREN qualified_identifier RPAREN RPAREN.
having_operand ::= selected_grouped_aggregate_expression.
```

## Runtime Semantics

The planner resolves the aggregate argument against MyLite descriptors,
verifies that it is an integer, nonbinary string, or binary string descriptor
column, and lowers integer and binary string arguments to SQLite as
`COUNT(DISTINCT "physical_column")`.
Nonbinary string arguments add MyLite's registered string-key collation inside
the distinct expression. SQLite owns source scanning, filtering, grouping,
distinct aggregation, `HAVING`, ordering, and limiting. MyLite owns descriptor
resolution, generated identifier quoting, parameter binding, result metadata,
diagnostics, and public result formatting.

`COUNT(DISTINCT column)` is non-nullable metadata and always returns a decimal
integer text value for each emitted group.

This implementation uses MyLite wrapper/translation logic over public SQLite
APIs. It does not require a SQLite fork hook.

## Non-Goals

This slice does not add:

- multiple distinct expressions;
- arbitrary expression distinct arguments beyond the companion grouped
  row-scalar slice;
- decimal, floating, temporal, enum, set, JSON, or per-expression collation
  distinct-count semantics;
- wider grouped source forms than the current base-table, multiple-key
  base-table, and two-source joined grouped aggregate envelopes;
- joined multiple-key grouped count-distinct combinations beyond the current
  joined grouped source envelope;
- aggregate expressions that appear only in `HAVING`;
- boolean-composed `HAVING` predicates;
- selected aggregate alias ordering beyond the currently supported count-like
  grouped aggregate alias order surface;
- executable aggregate windows.

Unsupported forms must continue to return deterministic MyLite diagnostics
rather than falling through to arbitrary SQLite SQL.
