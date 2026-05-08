# Natural joins spec

## Scope

Implement the MySQL 8.4 `NATURAL` join forms for supported MyLite SELECT
base-table join operands:

- `NATURAL JOIN`
- `NATURAL INNER JOIN`
- `NATURAL LEFT JOIN`
- `NATURAL LEFT OUTER JOIN`
- `NATURAL RIGHT JOIN`
- `NATURAL RIGHT OUTER JOIN`

The [MySQL 8.4 JOIN Clause](https://dev.mysql.com/doc/refman/8.4/en/join.html)
defines NATURAL joins as equivalent to a `USING` join over all columns that
have the same names in both joined operands. MyLite implements that rule for
the existing base-table inner and outer join runtime.

## Syntax

```lemon
joined_table_reference ::= joined_table_reference natural_inner_join_operator
    table_factor.
joined_table_reference ::= joined_table_reference natural_outer_join_operator
    table_factor.

natural_inner_join_operator ::= NATURAL JOIN.
natural_inner_join_operator ::= NATURAL INNER JOIN.

natural_outer_join_operator ::= NATURAL LEFT JOIN.
natural_outer_join_operator ::= NATURAL LEFT OUTER JOIN.
natural_outer_join_operator ::= NATURAL RIGHT JOIN.
natural_outer_join_operator ::= NATURAL RIGHT OUTER JOIN.
```

`NATURAL` joins do not accept an explicit `ON` or `USING` condition. MyLite
must reject `NATURAL JOIN ... ON ...` and `NATURAL JOIN ... USING (...)` at
parse time, matching MySQL syntax.

## Semantics

For a NATURAL join, MyLite discovers all common column names after table
columns have been loaded and after earlier `USING` joins in the left or right
operand have produced coalesced columns. The discovered names are then resolved
through the same validation and execution path as explicit `USING` joins.

If no common columns exist, a NATURAL inner join behaves as a Cartesian inner
join, and NATURAL outer joins behave as outer joins without equality predicates.

Common column comparison follows the existing `USING` equality behavior:

- rows match only when every common column pair compares as equal;
- `NULL` common-column values do not match each other;
- `LEFT` and `RIGHT` NATURAL joins apply the equality check before null
  extension, then apply `WHERE`, `ORDER BY`, grouping, and projection after
  null extension through the existing outer-join flow.

Common column names are matched case-insensitively, following the current
identifier matching behavior used by MyLite's explicit `USING` joins.

## Projection and metadata

Wildcard projection uses the same coalescing rules as explicit `USING` joins:

- NATURAL inner and left joins emit common columns in left operand order, then
  left-only columns, then right-only columns.
- NATURAL right joins emit common columns in right operand order, then
  right-only columns, then left-only columns.
- Unqualified references to common names resolve to the coalesced column.
- Qualified references to common names continue to resolve to the base-table
  columns.
- Result metadata for coalesced columns follows the existing `USING` join
  descriptor and nullability rules.

## Diagnostics

If common-column resolution finds an ambiguous column inside either operand,
MyLite returns the same error path as explicit `USING` joins:

- error 1052 / `23000`, `Column '<name>' in from clause is ambiguous`

Unsupported table-reference surfaces remain outside this slice and use their
existing diagnostics:

- derived tables and subqueries in `FROM`
- partitions
- `STRAIGHT_JOIN`
- ODBC escaped outer joins
- broad parenthesized comma-list table-reference groups

## Runtime notes

The AST represents NATURAL as a join-condition kind. During SELECT `FROM`
binding, runtime records an implicit `USING` request with a `natural` flag.
After all table columns are loaded, the request enumerates common column names
from the left and right table ranges, then delegates to explicit `USING`
resolution. Row matching, wildcard expansion, column metadata, and outer-join
null extension therefore reuse the existing join machinery.

## Tests

Coverage includes:

- parser acceptance for NATURAL inner, left, and right join forms;
- parser rejection for NATURAL joins followed by explicit `ON` or `USING`;
- NATURAL inner joins over two common columns;
- NATURAL left joins with null extension of unmatched right rows;
- NATURAL right joins with right-side wildcard/coalesced column ordering.
