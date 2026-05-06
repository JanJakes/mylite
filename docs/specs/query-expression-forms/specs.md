# Query Expression Forms

## Scope

This slice expands MyLite's executable query-expression surface beyond
`SELECT ... UNION ...`:

- standalone `TABLE tbl_name [ORDER BY ...] [LIMIT ...]`
- standalone `VALUES ROW(...) [, ROW(...)] [ORDER BY ...] [LIMIT ...]`
- `UNION`, `INTERSECT`, and `EXCEPT`, each with default `DISTINCT`, explicit
  `DISTINCT`, and explicit `ALL`
- MySQL's `INTERSECT` precedence over `UNION` and `EXCEPT`
- parenthesized query expressions with local `ORDER BY` and `LIMIT`
- `WITH` / `WITH RECURSIVE` CTE query-expression syntax as a parser
  placeholder

Out of scope:

- executable CTE name resolution, recursive iteration, materialization, and
  derived table plumbing
- query expressions as DML sources for `INSERT`, `REPLACE`, `UPDATE`, or
  `DELETE`
- optimizer choices, stable row order without a complete `ORDER BY`, and
  broader row-source support beyond currently executable MyLite statements
- exact metadata for all possible `VALUES` expression type combinations beyond
  the existing expression descriptor merger

## Sources

- MySQL 8.4 Reference Manual, Set Operations:
  https://dev.mysql.com/doc/refman/8.4/en/set-operations.html
- MySQL 8.4 Reference Manual, Parenthesized Query Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/parenthesized-query-expressions.html
- MySQL 8.4 Reference Manual, `WITH` common table expressions:
  https://dev.mysql.com/doc/refman/8.4/en/with.html

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

```text
docker exec -i mylite-mysql-849 mysql -uroot --password= --protocol=TCP --table --show-warnings
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

`TABLE t` is equivalent to selecting all visible columns from `t`, and accepts
global ordering and limiting:

```sql
TABLE left_t ORDER BY id LIMIT 2;
```

`VALUES` returns an anonymous row source. MySQL names its columns `column_0`,
`column_1`, and so on:

```sql
VALUES ROW(1,-2,3), ROW(5,7,9), ROW(4,6,8) ORDER BY column_1;
```

Set operations compare whole result rows. The default duplicate mode is
`DISTINCT`; `ALL` preserves multiset counts according to the operation:

- `INTERSECT DISTINCT` returns each row present on both sides once.
- `INTERSECT ALL` returns each row the minimum number of times it appears on
  either side.
- `EXCEPT DISTINCT` returns each left row that is absent from the right side
  once.
- `EXCEPT ALL` subtracts matching right-side occurrences from left-side
  occurrences.

`INTERSECT` binds more tightly than `UNION` and `EXCEPT`:

```sql
SELECT 1 AS v UNION SELECT 2 INTERSECT SELECT 2 ORDER BY v;
-- returns 1, 2
```

Parentheses override that precedence:

```sql
(SELECT 1 AS v UNION SELECT 2) EXCEPT SELECT 2 ORDER BY v;
-- returns 1
```

Each query expression operand must have the same column count. MySQL reports
error 1222, "The used SELECT statements have a different number of columns",
for mismatches.

## MyLite Behavior

### Parser and AST

The parser now represents query-expression structure explicitly:

- `MYLITE_SQL_AST_QUERY_EXPRESSION` owns the body plus global `ORDER BY` and
  `LIMIT` clauses.
- `MYLITE_SQL_AST_UNION_EXPRESSION` is used for all set operations and carries
  both the operation kind (`UNION`, `INTERSECT`, `EXCEPT`) and duplicate mode.
- `MYLITE_SQL_AST_QUERY_PRIMARY` preserves parentheses around query
  expressions so local clauses and precedence are not flattened away.
- `MYLITE_SQL_AST_VALUES_STATEMENT` represents standalone and operand
  `VALUES ROW(...)` query forms.

`TABLE tbl_name` is lowered to the existing `SELECT * FROM tbl_name` AST shape,
which keeps execution and metadata behavior aligned with the current table
select implementation.

`WITH` and `WITH RECURSIVE` are accepted as
`MYLITE_SQL_AST_PLACEHOLDER_CTE`. Preparing and stepping such statements
produces the standard parser-placeholder warning 1235 and no result columns or
rows. Full CTE execution waits for derived-table row sources and CTE name
resolution.

### Execution

`VALUES` statements are prepared as custom result statements:

- every row must contain at least one expression
- all rows must have the same column count
- output columns are named `column_0`, `column_1`, ...
- descriptors are inferred by merging each column's row expressions through the
  existing expression descriptor merger
- `ORDER BY` and `LIMIT` use the same global query-expression binding as set
  operations

Set-operation execution materializes operands into MyLite rowsets and applies
row equality through the existing output-value comparison logic, preserving
collation-aware equality where current descriptors support it.

Parenthesized query expressions execute as nested query-expression statements,
so local `ORDER BY` and `LIMIT` are applied before the outer set operation sees
the operand rows. A global `LIMIT 0` without `SQL_CALC_FOUND_ROWS` short-circuits
without evaluating discarded rows, matching the existing scalar/table SELECT
warning behavior.

### Diagnostics

Column-count mismatches in `VALUES` rows and set-operation operands report the
MySQL-compatible message and warning/error code 1222. CTE placeholders report
warning 1235 with a message stating that CTE query expressions are accepted but
not executed.

## Test Coverage

Parser tests cover:

- `TABLE`
- standalone `VALUES`
- `VALUES ... UNION TABLE ...`
- `INTERSECT` precedence
- `EXCEPT`
- parenthesized set expressions
- standalone parenthesized query expressions
- non-recursive and recursive CTE placeholders

Runtime tests cover:

- standalone `TABLE` with `ORDER BY` and `LIMIT`
- standalone `VALUES` with generated `column_N` labels
- `INTERSECT DISTINCT`
- `EXCEPT DISTINCT`
- `INTERSECT ALL` multiplicity
- `EXCEPT ALL` multiplicity
- `INTERSECT` precedence over `UNION`
- parenthesized set precedence
- `VALUES` operands in set operations
- column-count mismatch diagnostics
- CTE placeholder warning behavior
