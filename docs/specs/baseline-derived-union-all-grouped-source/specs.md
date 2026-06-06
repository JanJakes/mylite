# Baseline Derived UNION ALL Grouped Source

## Summary

This slice admits a narrow MySQL-compatible derived-table source shaped as an
unparenthesized `UNION ALL` chain inside a grouped aggregate query:

```sql
SELECT post_status, COUNT(*) AS num_posts
FROM (
  SELECT post_status FROM posts WHERE post_type = 'x' AND post_status != 'private'
  UNION ALL
  SELECT post_status FROM posts WHERE post_type = 'x' AND post_status = 'private'
) AS filtered_posts
GROUP BY post_status
```

The motivating query is WordPress `wp_count_posts(..., 'readable')`. MyLite
plans each branch through the existing descriptor-backed `SELECT` planner and
renders the derived `UNION ALL` source into SQLite SQL. It does not materialize
the derived rows in C.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Related MyLite specs:
  - `docs/specs/baseline-union-select-lifecycle/specs.md`
  - `docs/specs/baseline-derived-join-select/specs.md`
  - `docs/specs/baseline-group-by-string-column/specs.md`
- Official MySQL 8.4 Reference Manual:
  - derived tables: <https://dev.mysql.com/doc/refman/8.4/en/derived-tables.html>
  - set operations: <https://dev.mysql.com/doc/refman/8.4/en/set-operations.html>
  - `GROUP BY`: <https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Scope

Supported:

- a derived source in a plain grouped aggregate query;
- derived source body as `select_statement UNION ALL select_statement` with
  two or more branches;
- descriptor-backed branch `SELECT` plans that already work as standalone
  MyLite descriptor selects;
- no branch `ORDER BY`, branch `LIMIT`, locking clause, `SQL_CALC_FOUND_ROWS`,
  grouping, or set operator other than `UNION ALL`;
- derived output columns copied from the first branch, matching MySQL's derived
  column-name rule for the admitted shape;
- outer grouping, aggregate planning, `HAVING`, `ORDER BY`, and `LIMIT` through
  the existing grouped aggregate envelope;
- nested branch predicate parameter binding in SQL render order.

Unsupported:

- derived `UNION`, `UNION DISTINCT`, `INTERSECT`, or `EXCEPT`;
- parenthesized query expressions or global compound `ORDER BY` / `LIMIT`;
- grouped, aggregate, joined, scalar-expression, CTE, `TABLE`, or `VALUES`
  branch bodies;
- derived sources in DML paths beyond surfaces already explicitly supported;
- full MySQL type aggregation, collation aggregation, optimizer merge versus
  materialization behavior, privilege behavior, or protocol-grade origin
  metadata.

## Grammar

The parser admits `compound_select_statement` inside derived-table parentheses:

```lemon
derived_table_source(A) ::= LPAREN(L) compound_select_statement(S) RPAREN(R)
                            table_alias_opt(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}
```

This is an admission rule only. The planner narrows it to the `UNION ALL`
grouped-source subset described above.

## Semantics

Planning:

1. Require a derived-table alias.
2. Require every compound term to be `UNION ALL`.
3. Reject branch-local `ORDER BY`, `LIMIT`, locking clauses,
   `SQL_CALC_FOUND_ROWS`, and grouped branches.
4. Plan each branch through the existing descriptor `SELECT` planner.
5. Require all branches to expose the same column count.
6. Expose derived source descriptors from the first branch.
7. Plan the outer grouped aggregate against the synthetic derived source.

Execution:

- Generated SQLite SQL renders the derived source as a nested `SELECT ... UNION
  ALL SELECT ...` body with the usual generated MyLite source alias.
- Branch predicates bind before outer grouped predicates, group expressions,
  `HAVING`, `ORDER BY`, or `LIMIT`, matching the generated SQL order.
- Duplicate preservation is delegated to SQLite's `UNION ALL` execution for the
  rendered source.

## Tests

Coverage must include:

- a WordPress-shaped grouped aggregate over a derived `UNION ALL` source;
- branch predicates with bound values in both branches;
- first-branch column labels and grouped aggregate output;
- MySQL 8.4.9 runtime expectation coverage for the result rows.
