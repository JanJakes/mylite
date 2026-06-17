# Baseline Row-Scalar IN Predicates

## Summary

This slice admits supported row-scalar function expressions as the subject of
`WHERE ... IN (...)` and `WHERE ... NOT IN (...)` literal-list predicates. It
extends the same single-table descriptor-backed `WHERE` surface used by the
existing row-scalar comparison, `IS [NOT] NULL`, and `[NOT] BETWEEN`
predicates.

The slice covers list values that are either supported predicate literals or
supported row-scalar function expressions. It does not add `IN` subqueries for
row-scalar subjects, row constructors, aggregate/window expressions, or a
general expression virtual machine.

References:

- MySQL 8.4 Reference Manual, "Comparison Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- MySQL 8.4 Reference Manual, "Expressions":
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Runtime probes against MySQL `8.4.9` in the local `mylite-mysql-849`
  comparison container.

## MySQL 8.4.9 Behavior

MySQL accepts ordinary expression subjects and ordinary expression list entries
for `IN` predicates. Observed probes:

```sql
CREATE TABLE expr_pred(
    id INT PRIMARY KEY,
    i INT,
    v VARCHAR(64),
    b VARBINARY(16),
    js JSON,
    dt DATETIME
);
INSERT INTO expr_pred VALUES
    (1, 10, 'Alpha', UNHEX('4142'), JSON_OBJECT('a', 1), '2024-01-02 03:04:05'),
    (2, 0, 'beta', UNHEX('4344'), JSON_OBJECT('a', 2), '2024-01-03 04:05:06'),
    (3, NULL, NULL, NULL, NULL, NULL);

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE LOWER(v) IN ('alpha', 'gamma');
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE COALESCE(i, 0) IN (0, 10);
-- 1,2,3

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE HEX(b) NOT IN ('4142', 'FFFF');
-- 2

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE IFNULL(v, 'fallback') IN (LOWER(v), COALESCE(v, 'fallback'));
-- 1,2,3

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE IFNULL(v, 'fallback') IN ('fallback', COALESCE(v, 'fallback'));
-- 1,2,3

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')) IN ('1', '3');
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE DATEDIFF(dt, '2024-01-01') IN (1, 3);
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE MD5(v) IN (MD5('Alpha'), MD5('missing'));
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM expr_pred
WHERE UNCOMPRESSED_LENGTH(COMPRESS(v)) IN (0, 5);
-- 1
```

For `NULL`, MySQL follows three-valued predicate behavior: if the subject is
`NULL`, the predicate result is `NULL`; if no match is found and any list entry
is `NULL`, the predicate result is also `NULL`. In a `WHERE` filter, both
`FALSE` and `NULL` exclude the row.

## Supported Scope

MyLite supports descriptor-backed `WHERE` predicates where the left side is a
supported row-scalar expression and the predicate is:

- `IN` or `NOT IN`;
- a nonempty literal/function value list;
- a value list whose entries are supported predicate literals or supported
  row-scalar predicate value expressions.

The expression envelope is intentionally identical to the row-scalar comparison
predicate envelope for this phase. Existing row-scalar function support still
controls which argument shapes, descriptor column families, nested functions,
warnings, and result encodings are available.

## Intentionally Unsupported

This slice does not add:

- row-scalar `IN (SELECT ...)` subqueries;
- row constructor `IN` predicates;
- bare truth predicates outside the later
  [baseline row-scalar truth predicates](../baseline-row-scalar-truth-predicates/specs.md)
  slice;
- `HAVING`, generated-column, default-expression, aggregate/window, or broad
  DML assignment contexts;
- broad MySQL coercion beyond the current row-scalar predicate helpers;
- SQLite fork hooks.

## Grammar

The parser already admits the relevant Lemon shape:

```lemon
predicate_atom(A) ::= predicate_row_scalar_expression(C) IN(I)
        LPAREN predicate_in_value_list(V) RPAREN(R).
predicate_atom(A) ::= predicate_row_scalar_expression(C) NOT(N) IN(I)
        LPAREN predicate_in_value_list(V) RPAREN(R).

predicate_in_value(A) ::= predicate_integer_value(V).
predicate_in_value(A) ::= STRING(T).
predicate_in_value(A) ::= BIT_LITERAL(T).
predicate_in_value(A) ::= HEX_LITERAL(T).
predicate_in_value(A) ::= NULL(T).
predicate_in_value(A) ::= DECIMAL(T).
predicate_in_value(A) ::= FLOAT(T).
predicate_in_value(A) ::= supported_row_scalar_function(V).
```

This slice extends the `predicate_in_value` value envelope where needed so list
entries may include the same supported row-scalar functions as comparison RHS
values.

## Architecture

The implementation adds a row-scalar `IN` predicate plan node. The node owns:

- one planned row-scalar subject expression;
- a value count;
- a parallel value array for literal list entries;
- a parallel row-scalar expression array for function list entries.

SQL lowering renders the subject with the existing row-scalar SQL builder and
renders each list entry as either a bound parameter or a row-scalar SQL
expression. Parameter binding follows SQL order by binding subject expression
parameters, then list-entry function parameters or literal parameters.

SQLite evaluates `IN`/`NOT IN` directly. MyLite keeps ownership of function
semantics, diagnostics, and binary/text literal binding through its existing
row-scalar UDFs and predicate literal helpers.

## Tests

Tests cover:

- string, numeric, binary, JSON, temporal, and digest row-scalar `IN`
  predicates;
- `NOT IN` over a row-scalar binary/string function;
- mixed literal and row-scalar function entries inside the `IN` list;
- compression row-scalar expressions inside `IN`;
- MySQL-runtime-verified row sets for all covered contexts;
- focused MyLite C runtime assertions for the same contexts.
