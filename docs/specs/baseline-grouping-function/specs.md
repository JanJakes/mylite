# Baseline `GROUPING()` Function Specification

## Scope

This slice implements `GROUPING(group_key)` for MyLite's existing single-key
`GROUP BY ... WITH ROLLUP` baseline.

The supported executable form is:

```sql
SELECT [group_projection, ...] GROUPING(group_key) [AS alias], aggregate...
FROM source
[WHERE predicate]
GROUP BY group_key WITH ROLLUP
```

The `group_key` argument must resolve to the same descriptor column as the
single supported `GROUP BY` key. Ordinary grouped rows return integer text
`0`. The final super-aggregate rollup row returns integer text `1`. This
distinguishes a stored `NULL` group value from the rollup `NULL` marker.

`GROUPING(group_key)` may be selected with or without an alias. Without an
alias, the result column label is the function expression text, matching the
existing scalar-expression label behavior.

## MySQL 8.4.9 Behavior Verified

Official MySQL documentation for `GROUP BY` modifiers states that
`GROUPING()` is available in select lists, `HAVING`, and `ORDER BY` for
rollup queries, and returns `1` for super-aggregate `NULL` values and `0`
otherwise:

- <https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html>

Runtime probes against MySQL 8.4.9 verified:

- a stored `NULL` group row returns `GROUPING(g) = 0`;
- the final single-key rollup row returns `GROUPING(g) = 1`;
- `GROUPING(g)` without `WITH ROLLUP` fails with `1111 / HY000` and
  `Invalid use of group function`;
- `GROUPING(non_grouped_column)` fails with `3602 / HY000`;
- `GROUPING(non_column_expression)` fails with `1210 / HY000`.

## MyLite Compatibility Decisions

MyLite implements only the single-key select-list form in this slice because
multi-key rollup subtotals, rollup-aware `HAVING`, and rollup-aware `ORDER BY`
are not yet executable.

Out-of-scope forms return deterministic diagnostics:

- no `WITH ROLLUP`: `1111 / HY000`, `Invalid use of group function`;
- argument count other than one: unsupported diagnostic for this baseline
  envelope;
- non-column argument: `1210 / HY000`, `Incorrect arguments to GROUPING function`;
- column argument not present in the supported grouping keys: `3602 / HY000`,
  `Argument #1 of GROUPING function is not in GROUP BY`;
- `GROUPING()` in `HAVING`, `ORDER BY`, scalar, DML, or non-grouped contexts
  remains unsupported.

The implementation must not materialize source tables in MyLite. Normal grouped
rows render `GROUPING()` as a constant SQLite projection of `0`; the rollup row
appender writes `1` into grouping-marker result cells while continuing to ask
SQLite for aggregate totals.

## Tests

Coverage must include:

- ordinary `NULL` group plus rollup row marker values;
- marker-only projection with no aggregate result;
- alias and default result column labels;
- source `WHERE` filtering;
- explicit diagnostics for no rollup, non-column arguments, non-grouped
  arguments, and multi-argument forms.
