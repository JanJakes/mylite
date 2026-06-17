# Baseline Row-Scalar Truth Predicates

## Summary

This slice admits supported row-scalar function expressions as bare truth
predicates in descriptor-backed `WHERE` clauses. It extends the same
single-table predicate envelope already used for row-scalar comparison,
`IS [NOT] NULL`, `[NOT] BETWEEN`, and `[NOT] IN` predicates.

The slice does not add descriptor-column bare truth tests, broad arbitrary
expression predicates, subqueries, `HAVING`, grouping, DML assignment,
generated-column, or default-expression support.

References:

- MySQL 8.4 Reference Manual, "Expressions":
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL 8.4 Reference Manual, "Working with NULL Values":
  <https://dev.mysql.com/doc/refman/8.4/en/working-with-null.html>
- MySQL 8.4 Reference Manual, "Type Conversion in Expression Evaluation":
  <https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html>
- Runtime probes against MySQL `8.4.9` in the local `mylite-mysql-849`
  comparison container.

## MySQL 8.4.9 Behavior

MySQL expression grammar allows a predicate to reduce to a general expression.
In a truth context, `0` and `NULL` are false and other values are true.
String-valued expressions are converted numerically for truth evaluation.

Observed probes:

```sql
CREATE TABLE expr_pred(
    id INT PRIMARY KEY,
    i INT,
    v VARCHAR(64),
    b VARBINARY(16),
    js JSON,
    dt DATETIME,
    tm TIME
);
INSERT INTO expr_pred VALUES
    (1, 10, 'Alpha', UNHEX('4142'), JSON_OBJECT('a', 1),
        '2024-01-02 03:04:05', '00:00:59'),
    (2, 0, 'beta', UNHEX('4344'), JSON_OBJECT('a', 2),
        '2024-01-03 04:05:06', '00:01:01'),
    (3, NULL, NULL, NULL, NULL, NULL, NULL);

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred WHERE COALESCE(i, 0);
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred WHERE NOT COALESCE(i, 0);
-- 2,3

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred WHERE ISNULL(i);
-- 3

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred WHERE DATEDIFF(dt, '2024-01-02');
-- 2

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred WHERE JSON_UNQUOTE(JSON_EXTRACT(js, '$.a'));
-- 1,2

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred WHERE LOWER(v);
-- NULL aggregate result; no rows match

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred WHERE HEX(b);
-- 1,2
```

## Supported Scope

MyLite supports bare truth predicates when the predicate expression is accepted
by the current `predicate_row_scalar_expression` parser surface and row-scalar
planner. This includes the supported control-flow, concatenation, extrema,
field lookup, string transform, digest, compression, random-bytes, JSON,
numeric, and temporal predicate-expression subsets. Nested operands may use the
same supported value expressions already admitted by each function family, but
this slice does not add new top-level bare `CAST`, `CONVERT`, `COLLATE`,
charset/collation metadata, UUID, interval, or row-arithmetic truth predicates.

The expression is lowered to the existing row-scalar SQL builder and then left
as the SQLite `WHERE` term. The row-scalar UDF or translated expression remains
responsible for MySQL-compatible function behavior. SQLite's truth conversion
matches the admitted MySQL truth cases: `NULL` and numeric zero exclude the row,
nonzero numeric values include the row, and nonnumeric strings convert to zero
in truth context.

## Intentionally Unsupported

This slice does not add:

- bare descriptor-column truth predicates such as `WHERE column_name`;
- arbitrary expression truth predicates outside the supported row-scalar
  planner;
- bare truth predicates in `HAVING`, generated columns, defaults, CHECK
  constraints, aggregate/window expressions, or DML assignment contexts;
- broad warning-producing string-to-number truth conversions beyond the
  admitted row-scalar outputs;
- a SQLite fork hook.

## Grammar

The intended MyLite Lemon shape is:

```lemon
predicate_atom(A) ::= predicate_row_scalar_expression(C).
predicate_atom(A) ::= predicate_row_scalar_expression(C) comparison_operator(O) value(V).
predicate_atom(A) ::= predicate_row_scalar_expression(C) IS NULL.
predicate_atom(A) ::= predicate_row_scalar_expression(C) BETWEEN value(V) AND value(W).
predicate_atom(A) ::= predicate_row_scalar_expression(C) IN LPAREN value_list(V) RPAREN.
```

The new rule is the bare `predicate_row_scalar_expression` atom. The other
forms already existed before this slice.

## Architecture

The parser emits the row-scalar function AST node directly as the predicate
atom. Predicate planning recognizes supported row-scalar expressions and creates
a `PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH` node with one planned row-scalar
expression. Existing SQL generation renders only that expression, and existing
parameter binding binds its nested arguments in SQL order.

No storage format, catalog, public ABI, or SQLite fork changes are required.

## Tests

Tests cover:

- numeric/control-flow truth and `NOT` truth;
- `ISNULL()` truth over `NULL`;
- numeric extrema and arithmetic function truth;
- temporal, JSON, binary/string, digest, and compression row-scalar truth;
- MySQL-runtime-verified row sets for every covered query;
- focused MyLite C runtime assertions for the same row sets.
