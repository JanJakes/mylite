# Parser Aggregate Window Syntax

## Scope

This slice admits common MySQL 8.4.9 aggregate window syntax into MyLite's
parser while keeping execution explicitly unsupported unless a later runtime
slice implements correct window aggregate semantics.

Supported parser forms:

- `COUNT(*) OVER window_spec_or_name`
- `COUNT(column) OVER window_spec_or_name`
- `COUNT(DISTINCT column) OVER window_spec_or_name`, accepted so runtime can
  report unsupported-window behavior instead of a syntax error;
- `SUM(sum_argument) OVER window_spec_or_name`, where `sum_argument` is the
  existing MyLite `SUM()` aggregate argument subset;
- `AVG(column) OVER window_spec_or_name`
- `MIN(column) OVER window_spec_or_name`
- `MAX(column) OVER window_spec_or_name`
- `BIT_AND(column) OVER window_spec_or_name`
- `BIT_OR(column) OVER window_spec_or_name`
- `BIT_XOR(column) OVER window_spec_or_name`
- `GROUP_CONCAT(expr [ORDER BY ...] [SEPARATOR ...]) OVER window_spec_or_name`

The `OVER` clause reuses the existing expanded MyLite window grammar for
inline specs, named windows, partition/order lists, and `ROWS` / `RANGE`
frames.

Out of scope:

- execution of aggregate window functions;
- JSON aggregate windows, which are covered by the later
  [parser corpus JSON/statistical aggregate window surfaces](../parser-corpus-json-stat-aggregate-window-surfaces/specs.md)
  placeholder slice;
- statistical aggregate-window execution, which is covered by the later
  [baseline statistical aggregate window functions](../baseline-statistical-aggregate-window-functions/specs.md)
  slice;
- window-function result metadata beyond parser AST shape;
- real aggregate-window planning, SQLite SQL generation, frame evaluation, or
  MySQL exact numeric widening semantics.

## References

- Official MySQL 8.4 Reference Manual, window functions:
  <https://dev.mysql.com/doc/refman/8.4/en/window-functions.html>
- Official MySQL 8.4 Reference Manual, aggregate function descriptions:
  <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>
- Official MySQL 8.4 Reference Manual, window concepts and syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>

Runtime probes were run against the local `mysql:8.4.9` comparison container
`mylite-mysql-849`.

Observed MySQL 8.4.9 behavior:

- ordinary aggregate windows such as `COUNT(*) OVER ()`,
  `SUM(v) OVER (PARTITION BY grp ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING
  AND CURRENT ROW)`, and named-window `MIN()` / `MAX()` / `AVG()` forms are
  accepted;
- `COUNT(DISTINCT id) OVER ()` fails with `1235 / 42000` and the message
  indicating that MySQL does not yet support `<window function>(DISTINCT ..)`.

Probe command:

```sh
docker exec mylite-mysql-849 mysql -uroot --batch --raw --skip-column-names -e "
CREATE DATABASE IF NOT EXISTS mylite_probe;
USE mylite_probe;
DROP TABLE IF EXISTS aw;
CREATE TABLE aw(id INT, grp INT, v INT);
INSERT INTO aw VALUES (1,1,10),(2,1,20),(3,2,5);
SELECT id, COUNT(*) OVER () AS c,
       SUM(v) OVER (PARTITION BY grp ORDER BY id
                    ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS s
FROM aw ORDER BY id;
SELECT id, MIN(v) OVER w AS mn, MAX(v) OVER w AS mx, AVG(v) OVER w AS av
FROM aw WINDOW w AS (PARTITION BY grp ORDER BY id) ORDER BY id;
SELECT COUNT(DISTINCT id) OVER () FROM aw;"
```

## MyLite Grammar

Independent Lemon-shape grammar for this slice:

```lemon
aggregate_window_expression ::=
    COUNT LPAREN STAR RPAREN over_clause
aggregate_window_expression ::=
    COUNT LPAREN qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    COUNT LPAREN DISTINCT qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    COUNT LPAREN DISTINCT LPAREN qualified_identifier RPAREN RPAREN over_clause
aggregate_window_expression ::=
    SUM LPAREN sum_aggregate_argument RPAREN over_clause
aggregate_window_expression ::=
    AVG LPAREN qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    MIN LPAREN qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    MAX LPAREN qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    BIT_AND LPAREN qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    BIT_OR LPAREN qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    BIT_XOR LPAREN qualified_identifier RPAREN over_clause
aggregate_window_expression ::=
    GROUP_CONCAT LPAREN expression group_concat_order_opt
        group_concat_separator_opt RPAREN over_clause

expression ::= aggregate_window_expression
```

The AST reuses the existing aggregate-function node kinds and appends the
window spec/reference as the last child. Empty inline `OVER ()` clauses carry
an explicit empty `WINDOW_SPEC` child for aggregate functions so they remain
distinguishable from ordinary non-window aggregates. Runtime must treat an
aggregate node with a window child as an aggregate-window expression, not as an
ordinary aggregate.

## Runtime Semantics

Execution remains unsupported in this slice. When a parsed `SELECT` contains an
aggregate-window expression, MyLite returns a deterministic unsupported
diagnostic before ordinary aggregate planning so the statement cannot be
mis-executed as a grouped or scalar aggregate.

`EXPLAIN` query placeholders may still accept statements containing aggregate
window syntax because they do not execute or validate the child query.

## Tests

Focused tests should cover:

- parser acceptance for ordinary aggregate-window forms using inline and named
  windows;
- parser acceptance for `COUNT(DISTINCT ...) OVER ...`;
- parser acceptance for aggregate-window expressions participating in larger
  scalar expressions;
- runtime rejection for direct `SELECT COUNT(*) OVER ()` and
  `SELECT SUM(v) OVER (...) FROM t`;
- `EXPLAIN SELECT COUNT(*) OVER ()` still returning the placeholder result;
- parser corpus movement over the WordPress mysql-on-sqlite server-test query
  CSV.
