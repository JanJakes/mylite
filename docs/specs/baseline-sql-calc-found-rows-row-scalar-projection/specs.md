# Baseline SQL_CALC_FOUND_ROWS Row-Scalar Projection

## Summary

This slice extends the existing deprecated `SQL_CALC_FOUND_ROWS` support to a
narrow source-backed row-scalar `SELECT` shape used by WordPress:

```sql
SELECT SQL_CALC_FOUND_ROWS id, 1 AS marker
FROM posts
WHERE post_type = 'x'
ORDER BY post_date DESC
LIMIT 0, 10
```

The selected rows remain produced by the existing row-scalar projection engine.
The found-row state is computed as the number of source rows after source
resolution and `WHERE`, before `LIMIT` / `OFFSET`. MyLite keeps the count inside
SQLite through the existing descriptor-built `COUNT(*)` path and does not
materialize rows in C.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Related MyLite specs:
  - `docs/specs/baseline-found-rows-function/specs.md`
  - `docs/specs/baseline-joined-select-sql-calc-found-rows/specs.md`
- Official MySQL 8.4 Reference Manual:
  - information functions:
    <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
  - `SELECT`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_found_rows_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Scope

Supported:

- source-backed row-scalar `SELECT` statements already supported without
  `SQL_CALC_FOUND_ROWS`;
- descriptor columns, qualified wildcard expansion, and supported scalar
  literal projection items;
- optional existing row-scalar `WHERE`, `ORDER BY`, and `LIMIT` / `OFFSET`;
- joined row-scalar sources already admitted by the row-scalar planner;
- MySQL deprecation warning `1287` for successful statements;
- `FOUND_ROWS()` state updated to the matching pre-limit source row count.

Unsupported:

- tableless or `DUAL` row-scalar `SQL_CALC_FOUND_ROWS`;
- `DISTINCT` semantics beyond the current row-scalar support;
- row-scalar forms not already supported without `SQL_CALC_FOUND_ROWS`;
- CTEs, derived tables, set operations, or full optimizer behavior.

## Semantics

Planning records the independent `SQL_CALC_FOUND_ROWS` select flag on the
row-scalar plan. Source-backed row-scalar plans continue to use the existing
projection, predicate, ordering, and limit planners.

Execution runs the normal visible row-scalar query. When `SQL_CALC_FOUND_ROWS`
is present, MyLite runs the existing descriptor-built found-row `COUNT(*)`
query over the same source and predicate, without `ORDER BY`, `LIMIT`, or
`OFFSET`. The result object's found-row count is set from that count before
statement completion publishes it to connection-local `FOUND_ROWS()` state.

Successful `SQL_CALC_FOUND_ROWS` row-scalar statements append the same
deprecation warning as descriptor-backed SELECT statements. Failed statements
keep the existing failed-statement diagnostics policy.

## Tests

Coverage must include:

- a descriptor column plus integer literal projection with `WHERE`, `ORDER BY`,
  and `LIMIT`;
- qualified wildcard expansion plus integer literal projection with `LIMIT`;
- visible rows, warning count, and subsequent `FOUND_ROWS()` state;
- MySQL 8.4.9 expectation coverage for the same query shapes.
