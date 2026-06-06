# Baseline Scalar COUNT Subquery Predicates

## Scope

This slice admits the WordPress taxonomy query shape where a scalar aggregate
subquery is compared with an integer literal inside an outer `SELECT` `WHERE`
predicate:

```sql
SELECT outer_table.id
FROM outer_table
WHERE (
  SELECT COUNT(1)
  FROM inner_table
  WHERE inner_table.object_id = outer_table.id
) = 1
```

The supported subset is intentionally narrow:

- the scalar subquery must be parenthesized;
- the subquery must be a `SELECT` with one descriptor table source or the
  existing supported inner-join source subset;
- the select list must contain exactly `COUNT(*)` or `COUNT(literal)`;
- the optional subquery clause subset is `WHERE` only;
- the comparison operator subset is `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`,
  and `>=`;
- the comparison right-hand side must be an integer, signed integer, boolean,
  or exact quoted integer literal accepted by the existing scalar-literal
  predicate path;
- correlated inner predicates reuse the existing descriptor-column comparison
  support.

This slice does not implement general scalar subquery comparison predicates,
non-COUNT aggregate scalar subqueries, grouped scalar subqueries, scalar
subquery `ORDER BY`, `HAVING`, `LIMIT`, `UNION`, tableless aggregate subqueries,
or scalar subqueries in DML predicates.

## MySQL Behavior

MySQL 8.4.9 evaluates a scalar `COUNT()` subquery as a single-row aggregate.
`COUNT(*)` and `COUNT(1)` return a non-`NULL` integer count, including `0` when
no inner rows match. The outer predicate then applies the ordinary comparison
operator to that scalar value.

Runtime expectations are recorded in
`packages/libmylite/tests/mysql_baseline_in_subquery_predicates_expectations.sh`.

## MyLite Semantics

MyLite lowers the supported scalar `COUNT()` subquery comparison to a correlated
SQLite scalar aggregate subquery:

```sql
(SELECT COUNT(*) FROM "inner" AS _mylite_s1 WHERE ...) = ?
```

The subquery source, aliasing, joined-source validation, inner predicate
planning, source-index offsetting, and parameter binding reuse the existing
`EXISTS` subquery planning structure. The comparison literal is bound after all
parameters used by the inner subquery predicate.

## Grammar

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
predicate_atom ::= LPAREN select_statement RPAREN predicate_comparison_operator
                   predicate_comparison_value.
```

Runtime planning accepts the resulting scalar-subquery comparison only when the
subquery select list is the supported `COUNT()` shape.

## Compatibility

The feature is a supported subset of MySQL scalar subquery comparison semantics
for the WordPress taxonomy/category query pattern. Broader scalar subquery
predicate support remains unsupported until specified separately.
