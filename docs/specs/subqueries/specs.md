# Subqueries

## Scope

Task 29 specifies MySQL-compatible subquery grammar, binding, diagnostics, and
runtime behavior for the query and expression surfaces that MyLite already
executes. It starts subquery work; it does not mark subqueries implemented.

In scope for the feature design:

- scalar subqueries used as scalar operands
- scalar subqueries in comparison expressions, including `LIKE`
- row subqueries compared with row constructors
- `EXISTS` and `NOT EXISTS`
- scalar and row `IN` / `NOT IN` subqueries
- quantified comparisons with `ANY`, `SOME`, and `ALL`
- correlated subqueries and nested query-block name resolution
- cardinality, arity, unsupported-shape, and syntax diagnostics
- result metadata for scalar subqueries and predicate-valued subquery
  expressions
- interactions with the current `SELECT`, projection, `WHERE`, `ORDER BY`,
  `GROUP BY`, `HAVING`, inner join, outer join, and `DISTINCT` surfaces
- a scoped implementation plan for the next feature phase

The next implementation phase should make these executable first:

- subqueries inside `SELECT` statements over the existing supported row sources
- scalar subqueries in the projection list, `WHERE`, `ON`, `HAVING`, and
  `ORDER BY`
- `EXISTS`, `IN`, and quantified predicates in `WHERE`, `ON`, and `HAVING`
- row constructors and row `IN` subqueries where all row elements use the
  supported scalar expression subset
- correlated subqueries whose outer references resolve to the immediately or
  transitively enclosing query block
- subquery result sets made from the existing `SELECT` implementation:
  base tables, joins, grouping, `HAVING`, `ORDER BY`, `LIMIT`, and `DISTINCT`

Out of scope for the first executable slice:

- derived tables in `FROM`, `LATERAL`, table functions, and parenthesized
  query expressions as row sources
- `TABLE` and `VALUES` subqueries
- `UNION`, `INTERSECT`, `EXCEPT`, and recursive or non-recursive CTEs inside
  subqueries
- subqueries in `INSERT`, `UPDATE`, `DELETE`, `SET`, `DO`, stored programs,
  views, triggers, generated columns, default expressions, and check
  constraints
- self-referencing DML diagnostics such as update/delete target-table use
- optimizer transformations, semijoin planning, antijoin planning, derived
  materialization, index lookup, and decorrelation beyond what is needed for
  externally visible MySQL-compatible behavior
- optimizer hints, locking clauses, window functions, remaining function
  families, user variables, prepared parameters, and protocol packets beyond
  the current result descriptor API

MyLite may later widen subquery execution to DML and metadata statements, but
Task 29 should not claim those surfaces until they are separately implemented
and MySQL-runtime verified.

## Sources

- MySQL 8.4 Reference Manual, Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subqueries.html
- MySQL 8.4 Reference Manual, The Subquery as Scalar Operand:
  https://dev.mysql.com/doc/refman/8.4/en/scalar-subqueries.html
- MySQL 8.4 Reference Manual, Comparisons Using Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/comparisons-using-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ANY`, `IN`, or `SOME`:
  https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ALL`:
  https://dev.mysql.com/doc/refman/8.4/en/all-subqueries.html
- MySQL 8.4 Reference Manual, Row Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/row-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `EXISTS` or `NOT EXISTS`:
  https://dev.mysql.com/doc/refman/8.4/en/exists-and-not-exists-subqueries.html
- MySQL 8.4 Reference Manual, Correlated Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/correlated-subqueries.html
- MySQL 8.4 Reference Manual, Subquery Errors:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html
- MySQL 8.4 Reference Manual, Restrictions on Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/aggregate-grouping/specs.md`
  - `docs/specs/inner-joins/specs.md`
  - `docs/specs/outer-joins/specs.md`
  - `docs/specs/select-distinct/specs.md`
  - `docs/specs/subquery-in-predicates/specs.md`
  - `docs/specs/quantified-subquery-comparisons/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Fixture

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_task29_subqueries;
CREATE DATABASE mylite_task29_subqueries
  CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE mylite_task29_subqueries;

CREATE TABLE outer_t (
  id INT PRIMARY KEY,
  grp INT NULL,
  val INT NULL,
  txt VARCHAR(10) NULL
);

CREATE TABLE inner_t (
  id INT PRIMARY KEY,
  outer_id INT NULL,
  grp INT NULL,
  val INT NULL,
  txt VARCHAR(10) NULL
);

CREATE TABLE pair_t (
  a INT NULL,
  b INT NULL
);

INSERT INTO outer_t VALUES
  (1,1,10,'alpha'),
  (2,1,20,'beta'),
  (3,2,NULL,'gamma'),
  (4,NULL,5,NULL),
  (5,3,30,'delta');

INSERT INTO inner_t VALUES
  (101,1,1,10,'alpha'),
  (102,1,1,11,'alt'),
  (103,2,1,20,'beta'),
  (104,3,2,NULL,'gamma'),
  (105,NULL,2,99,'orphan'),
  (106,5,3,NULL,'nullval');

INSERT INTO pair_t VALUES
  (10,1),
  (20,1),
  (NULL,2),
  (30,3);
```

### Subquery Shapes

A subquery is a nested query block enclosed in parentheses. Its result shape is
constrained by the expression that consumes it:

- scalar contexts require exactly one column and at most one row
- row-comparison contexts require a single row with the same number of columns
  as the row constructor
- `IN` and quantified comparisons consume a one-column table subquery for
  scalar operands
- row `IN` consumes a table subquery with the same tuple width as the left row
  constructor
- `EXISTS` accepts any select list and only tests whether at least one row is
  produced

Subqueries can contain ordinary `SELECT` clauses such as `DISTINCT`, joins,
`WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, and `LIMIT`, subject to restrictions
on the context that consumes the subquery.

### Scalar Subqueries

A scalar subquery returns the only selected column from the only selected row.
If it produces no rows, the scalar value is `NULL`. If it produces more than
one row, execution fails with error 1242. If it produces more than one column,
validation fails with error 1241.

Representative results:

| SQL | Result |
| --- | --- |
| `SELECT (SELECT txt FROM inner_t WHERE id=101)` | `'alpha'` |
| `SELECT (SELECT txt FROM inner_t WHERE id=999)` | `NULL` |
| `SELECT (SELECT val FROM inner_t WHERE grp=1)` | error 1242 / `21000` |
| `SELECT (SELECT val, txt FROM inner_t WHERE id=101)` | error 1241 / `21000` |
| `SELECT (SELECT val FROM inner_t WHERE grp=1 ORDER BY val DESC LIMIT 1)` | `20` |

Scalar subqueries can be used inside larger expressions:

| SQL | Result |
| --- | --- |
| `SELECT (SELECT val FROM inner_t WHERE id=101) + 1` | `11` |
| `SELECT id FROM outer_t WHERE txt LIKE (SELECT 'a%') ORDER BY id` | `1` |

### EXISTS and NOT EXISTS

`EXISTS (subquery)` returns `1` when the subquery returns at least one row and
`0` when it returns no rows. `NOT EXISTS` is the logical negation. MySQL does
not use the subquery select list to decide existence; a row containing only
`NULL` values still makes `EXISTS` true.

Representative results:

| SQL | Result |
| --- | --- |
| `SELECT EXISTS (SELECT NULL FROM inner_t WHERE val IS NULL)` | `1` |
| `SELECT NOT EXISTS (SELECT * FROM inner_t WHERE id=999)` | `1` |
| `SELECT id FROM outer_t o WHERE EXISTS (SELECT 1 FROM inner_t i WHERE i.outer_id=o.id) ORDER BY id` | `1`, `2`, `3`, `5` |

### IN and NOT IN Subqueries

For scalar operands, `IN (subquery)` behaves like `= ANY (subquery)`.
`NOT IN` behaves like `<> ALL`, not like `<> ANY`.

Representative results:

| SQL | Result |
| --- | --- |
| `SELECT id FROM outer_t WHERE val IN (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `1`, `2` |
| `SELECT 7 IN (SELECT val FROM inner_t WHERE grp=3)` | `NULL` |
| `SELECT 7 NOT IN (SELECT val FROM inner_t WHERE grp=3)` | `NULL` |
| `SELECT 7 IN (SELECT val FROM inner_t WHERE grp=999)` | `0` |
| `SELECT 7 NOT IN (SELECT val FROM inner_t WHERE grp=999)` | `1` |

The `NULL` cases matter. If no equality match exists and at least one compared
value is unknown, the result is `NULL`, and a `WHERE` predicate filters the row
out.

### Quantified Comparisons

`ANY` and `SOME` are synonyms. They return true if at least one comparison is
true, false for an empty subquery, false when every comparison is false, and
`NULL` when no comparison is true but at least one comparison is unknown.

`ALL` returns true for an empty subquery, false if any comparison is false, and
`NULL` when no comparison is false but at least one comparison is unknown.

Representative results:

| SQL | Result |
| --- | --- |
| `SELECT id FROM outer_t WHERE val > ANY (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `2`, `5` |
| `SELECT id FROM outer_t WHERE val <> SOME (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `1`, `2`, `4`, `5` |
| `SELECT id FROM outer_t WHERE val >= ALL (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `2`, `5` |
| `SELECT 7 > ANY (SELECT val FROM inner_t WHERE grp=999)` | `0` |
| `SELECT 7 > ALL (SELECT val FROM inner_t WHERE grp=999)` | `1` |
| `SELECT 7 > ANY (SELECT val FROM inner_t WHERE grp=3)` | `NULL` |
| `SELECT 7 > ALL (SELECT val FROM inner_t WHERE grp=3)` | `NULL` |
| `SELECT 7 < ANY (SELECT val FROM inner_t WHERE grp IN (2,3))` | `1` |
| `SELECT 7 < ALL (SELECT val FROM inner_t WHERE grp IN (2,3))` | `NULL` |

MySQL 8.4.9 accepts row `= ANY` and row `= SOME` in the same way it accepts
row `IN`; other row quantified comparisons observed with multi-column
subqueries fail with error 1241:

| SQL | Result |
| --- | --- |
| `SELECT id FROM outer_t WHERE (val,grp) = ANY (SELECT a,b FROM pair_t) ORDER BY id` | `1`, `2`, `5` |
| `SELECT id FROM outer_t WHERE (val,grp) = SOME (SELECT a,b FROM pair_t) ORDER BY id` | `1`, `2`, `5` |
| `SELECT id FROM outer_t WHERE (val,grp) > ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000` |
| `SELECT id FROM outer_t WHERE (val,grp) <> ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000` |
| `SELECT id FROM outer_t WHERE (val,grp) = ALL (SELECT a,b FROM pair_t)` | error 1241 / `21000` |

The first implementation may defer row `= ANY` / `= SOME` if row `IN` is
already covered, but it must document the gap explicitly and must not reject
valid scalar quantified comparisons.

### Row Subqueries

Row subqueries are used with row constructors such as `(a,b)` or
`ROW(a,b)`. The left row constructor and subquery row must have the same
number of values.

Representative results:

| SQL | Result |
| --- | --- |
| `SELECT id FROM outer_t o WHERE (o.val,o.grp) = (SELECT p.a,p.b FROM pair_t p WHERE p.a=10) ORDER BY id` | `1` |
| `SELECT id FROM outer_t o WHERE (o.val,o.grp) IN (SELECT p.a,p.b FROM pair_t p) ORDER BY id` | `1`, `2`, `5` |
| `SELECT ROW(1,2) = (SELECT a,b FROM pair_t WHERE a=999)` | `NULL` |
| `SELECT (NULL,2) IN (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (7,2) NOT IN (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT id FROM outer_t WHERE (val,grp) = (SELECT val FROM inner_t WHERE id=101)` | error 1241 / `21000` |
| `SELECT id FROM outer_t WHERE (val,grp) = (SELECT val,grp FROM inner_t WHERE grp=1)` | error 1242 / `21000` |
| `SELECT id FROM outer_t WHERE ROW(val) = (SELECT val FROM inner_t WHERE id=101)` | syntax error 1064 / `42000` |

MySQL treats `ROW(1)` differently from multi-element row constructors in this
subquery comparison context. MyLite should reject one-element `ROW(...)`
comparisons until an implementation proves every one-element row-constructor
case against MySQL.

### Correlation and Name Resolution

A correlated subquery can refer to tables from an enclosing query block. MySQL
resolves names from the innermost query block outward. Inner aliases shadow
outer aliases that use the same name.

Representative results:

| SQL | Result |
| --- | --- |
| `SELECT id FROM outer_t o WHERE EXISTS (SELECT 1 FROM inner_t i WHERE i.outer_id=o.id) ORDER BY id` | `1`, `2`, `3`, `5` |
| `SELECT id, (SELECT COUNT(*) FROM inner_t i WHERE i.grp=o.grp) AS c FROM outer_t o ORDER BY id` | `(1,3)`, `(2,3)`, `(3,2)`, `(4,0)`, `(5,1)` |
| `SELECT id, (SELECT val FROM inner_t i WHERE i.outer_id=o.id AND i.id=999) AS s FROM outer_t o ORDER BY id` | `s` is `NULL` for every row |

Logical execution reevaluates a correlated subquery for each relevant outer
row. MyLite may cache uncorrelated subqueries, but only when the plan has no
outer references and no side effect or warning-order behavior that would become
observable.

### Context Restrictions and Diagnostics

Subqueries consumed by `IN`, `ALL`, `ANY`, or `SOME` cannot include `LIMIT` in
MySQL 8.4.9. Scalar subqueries can use `ORDER BY` and `LIMIT`.

Representative diagnostics:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT id FROM outer_t WHERE val IN (SELECT val FROM inner_t ORDER BY val LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` |
| `SELECT id FROM outer_t WHERE (val,grp) IN (SELECT a,b FROM pair_t LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` |
| `SELECT id FROM outer_t WHERE val = (SELECT val FROM inner_t ORDER BY val LIMIT 1)` | accepted as a scalar-subquery comparison |

Subquery-specific errors to preserve:

| Code | SQLSTATE | Meaning |
| --- | --- | --- |
| 1235 | `42000` | unsupported `LIMIT` in `IN`/`ALL`/`ANY`/`SOME` subquery |
| 1241 | `21000` | subquery returns the wrong number of columns for the consuming expression |
| 1242 | `21000` | scalar or row subquery returns more than one row |
| 1093 | `HY000` | DML target table is also read by a prohibited subquery shape; deferred from the first slice |
| 1064 | `42000` | syntactically invalid subquery or row-constructor shape |

For transactional statements, a subquery failure should fail the whole
statement. In the first SELECT-only slice, this means returning no partial
result set and leaving statement diagnostics set to the MySQL-compatible error.
DML rollback and target-table restrictions remain deferred.

### Metadata

Scalar subqueries are expression values:

- result label follows normal projection label rules: explicit alias first,
  otherwise expression text
- origin database, table, and column metadata are empty because the scalar
  subquery output is not exposed as a base-table column
- type, length, decimals, charset, and collation derive from the selected
  expression inside the subquery
- scalar subquery results are nullable even when the selected column is
  declared `NOT NULL`, because an empty subquery returns `NULL`

Observed metadata examples from `mysql --column-type-info -vvv`:

| SQL | Metadata |
| --- | --- |
| `SELECT (SELECT txt FROM inner_t WHERE id=101) AS scalar_txt LIMIT 0` | label `scalar_txt`, no origin table, `VAR_STRING`, length `10` |
| `SELECT (SELECT val FROM inner_t WHERE id=101) + 1 AS scalar_plus LIMIT 0` | label `scalar_plus`, `LONGLONG`, binary numeric, length `12`, decimals `0` |

Predicate-valued subquery expressions use MySQL integer boolean metadata:

| SQL | Metadata |
| --- | --- |
| `SELECT EXISTS (SELECT NULL FROM inner_t WHERE val IS NULL) AS exists_result LIMIT 0` | `LONGLONG`, binary numeric, length `1`, no origin |
| `SELECT val IN (SELECT val FROM inner_t WHERE grp=1) AS in_result FROM outer_t LIMIT 0` | `LONGLONG`, binary numeric, length `1`, no origin |
| `SELECT val > ANY (SELECT val FROM inner_t WHERE grp=1) AS any_result FROM outer_t LIMIT 0` | `LONGLONG`, binary numeric, length `1`, no origin |
| `SELECT (val,grp) IN (SELECT a,b FROM pair_t) AS row_in_result FROM outer_t LIMIT 0` | `LONGLONG`, binary numeric, length `1`, no origin |

MyLite should use the Task 23 result descriptor machinery for these expressions
and should not expose hidden subquery columns as output fields.

## Interactions With Existing MyLite Features

### SELECT and Projection

Scalar subqueries and predicate-valued subquery expressions can appear in the
projection list. They participate in projection aliasing, duplicate labels, and
expression metadata like any other expression. The projected value is evaluated
for each output row after the outer row is available. For no-table `SELECT`,
uncorrelated subqueries are evaluated against their own query block.

### WHERE, ON, and HAVING

Subquery predicates in `WHERE`, join `ON`, and `HAVING` use MySQL three-valued
truth filtering. Only true keeps the row or joined row pair. False and `NULL`
filter it out.

The `ON` scope must include the tables visible to the explicit join operand
that owns the `ON` expression. A subquery inside `ON` can reference those outer
tables. It must not accidentally see comma-left tables that MyLite already
keeps out of the explicit join operand scope.

`HAVING` subqueries run after grouping. They can reference grouped output or
aggregate-compatible expressions according to the same name-resolution and
`ONLY_FULL_GROUP_BY` rules used by the existing aggregate implementation.

### ORDER BY and LIMIT

Scalar subqueries and predicate-valued subquery expressions can be hidden
`ORDER BY` keys when the outer query permits hidden order keys. For
`SELECT DISTINCT`, Task 28's hidden-order restrictions still apply: a hidden
order expression that references a non-selected base column must produce error
3065 even if that reference appears inside a scalar subquery.

`LIMIT` in the outer query does not relax subquery cardinality checks for rows
that must be evaluated before limiting. A scalar subquery with its own `LIMIT`
is legal. A subquery consumed by `IN`, `ALL`, `ANY`, or `SOME` with its own
`LIMIT` fails with error 1235.

### GROUP BY and Aggregates

Scalar subqueries can appear in grouping expressions and aggregate query output
when MySQL permits the referenced columns. Aggregate functions inside
subqueries belong to the subquery query block, not to the outer query block,
unless a later feature explicitly implements MySQL's more advanced aggregate
outer-reference cases.

For the first implementation slice, keep support conservative:

- allow uncorrelated scalar subqueries in grouped projection, `HAVING`, and
  `ORDER BY`
- allow correlated scalar subqueries whose outer references are grouped columns
  or otherwise accepted by the existing aggregate analyzer
- reject unsupported aggregate/correlation combinations with deterministic
  MySQL-compatible diagnostics rather than mis-evaluating them

### Joins

Subqueries can reference any table visible in the current outer query block,
including tables from inner, comma, left, and right joins. Null-extended values
from outer joins are the values seen by subqueries evaluated after the join row
is formed. Subqueries inside `ON` are evaluated before outer-join null
extension, because the `ON` predicate itself is evaluated there.

Derived tables in `FROM` remain deferred. Task 29 should not introduce a
derived-table row-source node as a side effect of expression subquery work.

### DISTINCT

`DISTINCT` duplicate elimination compares projected row values after scalar
subqueries and predicate-valued subquery expressions have been evaluated.
Subquery internals are not visible in result metadata and do not add hidden
columns to the duplicate key.

### DML

The expression engine should be designed so `UPDATE`, `DELETE`, and future
`INSERT ... SELECT` can reuse subquery support, but first-slice execution should
not widen DML behavior unless DML tests cover rows, affected rows, warnings,
rollbacks, target-table restrictions, and diagnostics against MySQL 8.4.9.

## MyLite Parser and AST Design

Subqueries need to be represented as nested query blocks, not string fragments
or direct SQLite snippets. The parser should preserve the complete inner
`SELECT` AST so the analyzer can bind it with a parent query-block scope.

Recommended AST node additions:

- `MYLITE_SQL_AST_SUBQUERY_EXPRESSION`
- `MYLITE_SQL_AST_EXISTS_EXPRESSION`
- `MYLITE_SQL_AST_QUANTIFIED_COMPARISON`
- `MYLITE_SQL_AST_ROW_CONSTRUCTOR`
- `MYLITE_SQL_AST_SUBQUERY_VALUE_LIST`, if the evaluator needs an explicit
  table-subquery value wrapper

Recommended enums:

```c
enum mylite_sql_ast_subquery_quantifier {
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_NONE = 0,
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY = 1,
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME = 2,
    MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL = 3,
};

enum mylite_sql_ast_subquery_value_shape {
    MYLITE_SQL_AST_SUBQUERY_VALUE_SCALAR = 0,
    MYLITE_SQL_AST_SUBQUERY_VALUE_ROW = 1,
    MYLITE_SQL_AST_SUBQUERY_VALUE_TABLE = 2,
};
```

Recommended child order:

- scalar subquery: child 0 is the inner `SELECT`
- `EXISTS`: child 0 is the inner `SELECT`
- quantified comparison: child 0 is the left operand, child 1 is the inner
  `SELECT`; node metadata stores the comparison operator and quantifier
- `IN` subquery: child 0 is the left operand, child 1 is the inner `SELECT`;
  node metadata stores `IN` or `NOT IN`
- row constructor: children are row elements in source order

`ANY` and `SOME` are nonreserved MySQL keywords. MyLite should add parser
tokens for them with `IDENTIFIER` fallback so they remain usable as ordinary
identifiers outside quantified-comparison contexts.

## Lemon-Style Grammar Snippets

These snippets describe MyLite's intended Task 29 grammar shape. They are
independently authored and are not copied from MySQL grammar.

```lemon
%type subquery_quantifier { enum mylite_sql_ast_subquery_quantifier }
%type comparison_operator { enum mylite_sql_ast_operator }

primary_expression(A) ::= scalar_subquery(B). {
    A = B;
}
primary_expression(A) ::= EXISTS(T) subquery(B). {
    A = mylite_sql_parser_make_exists_expression(state, T, B, false);
}
primary_expression(A) ::= row_constructor(B). {
    A = B;
}

logical_not_expression(A) ::= NOT(T) EXISTS subquery(B). {
    A = mylite_sql_parser_make_exists_expression(state, T, B, true);
}

scalar_subquery(A) ::= subquery(B). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, B);
}

subquery(A) ::= LPAREN(L) select_statement(B) RPAREN(R). {
    A = mylite_sql_parser_make_subquery(state, L, B, R);
}

row_constructor(A) ::= LPAREN(L) expression(B) COMMA expression_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_row_constructor(state, L, B, C, R);
}
row_constructor(A) ::= ROW(T) LPAREN expression(B) COMMA expression_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_row_constructor(state, T, B, C, R);
}

comparison_operand(A) ::= bit_or_expression(B). {
    A = B;
}
comparison_operand(A) ::= row_constructor(B). {
    A = B;
}

comparison_expression(A) ::= comparison_operand(B) IN(T) subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IN, C);
}
comparison_expression(A) ::= comparison_operand(B) NOT(T) IN subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_IN, C);
}

comparison_expression(A) ::= comparison_operand(B) comparison_operator(C)
        subquery_quantifier(D) subquery(E). {
    A = mylite_sql_parser_make_quantified_comparison(
        state, B, C, D, E);
}

comparison_operator(A) ::= EQ. {
    A = MYLITE_SQL_AST_OPERATOR_EQUAL;
}
comparison_operator(A) ::= NULL_SAFE_EQ. {
    A = MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL;
}
comparison_operator(A) ::= NE. {
    A = MYLITE_SQL_AST_OPERATOR_NOT_EQUAL;
}
comparison_operator(A) ::= LT. {
    A = MYLITE_SQL_AST_OPERATOR_LESS;
}
comparison_operator(A) ::= LE. {
    A = MYLITE_SQL_AST_OPERATOR_LESS_EQUAL;
}
comparison_operator(A) ::= GT. {
    A = MYLITE_SQL_AST_OPERATOR_GREATER;
}
comparison_operator(A) ::= GE. {
    A = MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL;
}

subquery_quantifier(A) ::= ANY. {
    A = MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY;
}
subquery_quantifier(A) ::= SOME. {
    A = MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME;
}
subquery_quantifier(A) ::= ALL. {
    A = MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL;
}
```

The implementation must resolve parser conflicts with existing parenthesized
expressions by recognizing a row constructor only when the parenthesized
expression contains a comma at the row-constructor level. `ROW(expr)` should
remain rejected in subquery-comparison contexts unless separately verified and
implemented.

Deferred grammar should remain rejected or explicitly unsupported:

```lemon
/* Deferred: table and values subqueries. */
subquery ::= LPAREN TABLE table_name RPAREN.
subquery ::= LPAREN VALUES row_value_list RPAREN.

/* Deferred: set operations and CTEs in subqueries. */
subquery ::= LPAREN WITH common_table_expression_list select_statement RPAREN.
subquery ::= LPAREN query_expression UNION query_expression RPAREN.

/* Deferred: derived-table row sources. */
table_factor ::= subquery table_alias.
table_factor ::= LATERAL subquery table_alias.
```

## Runtime Design

### Query Blocks and Binding

The analyzer should build a query-block stack. Each query block owns:

1. visible table aliases and base table names
2. output expressions and aliases
3. aggregate/grouping state
4. resolved outer references used by nested query blocks
5. statement diagnostics and warning propagation hooks

Name resolution inside a subquery first checks the subquery's own query block.
If a name is not found there, resolution walks outward through parent blocks.
Resolved outer references should store the target query-block depth and table
slot so execution does not repeat name lookup for every row.

### Evaluation Model

Subquery evaluation should use MyLite's existing SELECT planner and row
materialization path wherever possible:

- scalar subqueries fetch at most two rows and one column
- row scalar subqueries fetch at most two rows and the required number of
  columns
- `EXISTS` can stop after the first row
- `IN` and quantified predicates may materialize the inner values or stream
  them, as long as comparison order, warnings, errors, and NULL results remain
  MySQL-compatible
- correlated subqueries receive a read-only outer-row frame for each outer
  evaluation

Uncorrelated subqueries can be cached per statement execution only after
validation proves they do not depend on nondeterministic functions, user
variables, diagnostics order, or other observable side effects. Because many
of those features are not implemented yet, the first slice may simply evaluate
uncorrelated subqueries each time and optimize later.

### Cardinality and Arity Checks

Prepare-time checks should validate known output column counts:

- scalar subquery consumers require one column
- scalar `IN` and scalar quantified comparisons require one column
- row subquery consumers require the same number of columns as the row
  constructor
- row `IN` requires subquery tuple width to match the left tuple width
- row `= ANY` and `= SOME`, if implemented, follow row `IN` width checks

Execution-time checks should validate row counts:

- scalar and row scalar subqueries return `NULL` when empty
- scalar and row scalar subqueries fail with 1242 when a second row exists
- `IN`, `EXISTS`, and quantified predicates accept zero, one, or many rows

### NULL and Comparison Semantics

Subquery comparisons must reuse the Task 16 expression comparison and
conversion machinery:

- a true comparison determines `ANY` / `SOME`
- a false comparison determines `ALL`
- a remaining unknown comparison makes the result `NULL`
- `IN` and `NOT IN` must retain NULL-aware membership behavior
- row comparisons compare tuple elements left to right using MySQL row
  comparison rules
- warnings generated by conversions inside comparisons belong to the outer
  statement's warning list

### Diagnostics

Diagnostics should use MySQL-compatible codes and SQLSTATE values. Error
messages should match the observed MySQL strings closely enough for client
compatibility tests:

- 1235 / `42000`: unsupported `LIMIT & IN/ALL/ANY/SOME subquery`
- 1241 / `21000`: operand should contain the required number of columns
- 1242 / `21000`: subquery returns more than one row
- 1054 / `42S22`: unknown column in a subquery or outer reference
- 1052 / `23000`: ambiguous unqualified column after joins make ambiguity
  possible
- 1093 / `HY000`: deferred DML target-table self-reference restriction

The warning lifecycle should follow existing statement diagnostics: a failed
statement records the error condition; successful statements record warnings
from expression evaluation.

### Storage and Performance

Task 29 does not require new persistent storage. It does require statement
owned temporary value buffers for:

- scalar subquery results
- row-constructor values
- materialized `IN` or quantified subquery rows
- correlated outer-reference frames

The first implementation may use straightforward nested-loop evaluation. It
should keep the design ready for semijoin, antijoin, and uncorrelated-subquery
caching later without exposing optimizer-specific behavior.

## Test Plan

### Parser Tests

Add parser coverage for accepted shapes:

| SQL | Expected parser outcome |
| --- | --- |
| `SELECT (SELECT 1);` | scalar subquery expression |
| `SELECT id FROM outer_t WHERE val = (SELECT val FROM inner_t WHERE id=101);` | scalar comparison subquery |
| `SELECT id FROM outer_t WHERE EXISTS (SELECT 1 FROM inner_t);` | `EXISTS` expression |
| `SELECT id FROM outer_t WHERE NOT EXISTS (SELECT 1 FROM inner_t);` | negated existence expression |
| `SELECT id FROM outer_t WHERE val IN (SELECT val FROM inner_t);` | scalar `IN` subquery |
| `SELECT id FROM outer_t WHERE val NOT IN (SELECT val FROM inner_t);` | scalar `NOT IN` subquery |
| `SELECT id FROM outer_t WHERE val > ANY (SELECT val FROM inner_t);` | quantified comparison |
| `SELECT id FROM outer_t WHERE val <> SOME (SELECT val FROM inner_t);` | `SOME` quantifier |
| `SELECT id FROM outer_t WHERE val >= ALL (SELECT val FROM inner_t);` | `ALL` quantifier |
| `SELECT id FROM outer_t WHERE (val,grp) IN (SELECT a,b FROM pair_t);` | row `IN` subquery |
| `SELECT id FROM outer_t WHERE ROW(val,grp) = (SELECT a,b FROM pair_t WHERE a=10);` | row scalar comparison |
| `SELECT id FROM outer_t JOIN inner_t i ON EXISTS (SELECT 1);` | subquery in `ON` |
| `SELECT grp, COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) > (SELECT 1);` | subquery in `HAVING` |
| `SELECT id FROM outer_t ORDER BY (SELECT val FROM inner_t WHERE id=101);` | subquery order key |

Add parser rejection coverage:

| SQL | Expected parser outcome |
| --- | --- |
| `SELECT ROW(1) = (SELECT val FROM inner_t);` | syntax error until one-element row semantics are implemented |
| `SELECT id FROM outer_t WHERE val IN ();` | existing empty-list syntax error remains |
| `SELECT id FROM outer_t WHERE EXISTS SELECT 1;` | syntax error; subquery parentheses required |
| `SELECT id FROM outer_t WHERE val > ANY SELECT val FROM inner_t;` | syntax error; subquery parentheses required |

### Runtime Result Tests

Use the fixture above and compare rows against MySQL 8.4.9:

| SQL | Expected result |
| --- | --- |
| `SELECT (SELECT txt FROM inner_t WHERE id=101)` | `'alpha'` |
| `SELECT (SELECT txt FROM inner_t WHERE id=999)` | `NULL` |
| `SELECT (SELECT val FROM inner_t WHERE grp=1 ORDER BY val DESC LIMIT 1)` | `20` |
| `SELECT EXISTS (SELECT NULL FROM inner_t WHERE val IS NULL)` | `1` |
| `SELECT NOT EXISTS (SELECT * FROM inner_t WHERE id=999)` | `1` |
| `SELECT id FROM outer_t o WHERE EXISTS (SELECT 1 FROM inner_t i WHERE i.outer_id=o.id) ORDER BY id` | `1`, `2`, `3`, `5` |
| `SELECT id, (SELECT COUNT(*) FROM inner_t i WHERE i.grp=o.grp) AS c FROM outer_t o ORDER BY id` | `(1,3)`, `(2,3)`, `(3,2)`, `(4,0)`, `(5,1)` |
| `SELECT id FROM outer_t WHERE val IN (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `1`, `2` |
| `SELECT 7 IN (SELECT val FROM inner_t WHERE grp=3)` | `NULL` |
| `SELECT 7 NOT IN (SELECT val FROM inner_t WHERE grp=3)` | `NULL` |
| `SELECT 7 IN (SELECT val FROM inner_t WHERE grp=999)` | `0` |
| `SELECT 7 NOT IN (SELECT val FROM inner_t WHERE grp=999)` | `1` |
| `SELECT id FROM outer_t WHERE val > ANY (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `2`, `5` |
| `SELECT id FROM outer_t WHERE val <> SOME (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `1`, `2`, `4`, `5` |
| `SELECT id FROM outer_t WHERE val >= ALL (SELECT val FROM inner_t WHERE grp=1) ORDER BY id` | `2`, `5` |
| `SELECT 7 > ANY (SELECT val FROM inner_t WHERE grp=999)` | `0` |
| `SELECT 7 > ALL (SELECT val FROM inner_t WHERE grp=999)` | `1` |
| `SELECT id FROM outer_t o WHERE (o.val,o.grp) = (SELECT p.a,p.b FROM pair_t p WHERE p.a=10) ORDER BY id` | `1` |
| `SELECT id FROM outer_t o WHERE (o.val,o.grp) IN (SELECT p.a,p.b FROM pair_t p) ORDER BY id` | `1`, `2`, `5` |
| `SELECT ROW(1,2) = (SELECT a,b FROM pair_t WHERE a=999)` | `NULL` |
| `SELECT (NULL,2) IN (SELECT a,b FROM pair_t)` | `NULL` |

### Runtime Diagnostic Tests

| SQL | Expected behavior |
| --- | --- |
| `SELECT (SELECT val FROM inner_t WHERE grp=1)` | error 1242 / `21000` |
| `SELECT (SELECT val, txt FROM inner_t WHERE id=101)` | error 1241 / `21000` |
| `SELECT id FROM outer_t WHERE (val,grp) = (SELECT val FROM inner_t WHERE id=101)` | error 1241 / `21000` |
| `SELECT id FROM outer_t WHERE (val,grp) = (SELECT val,grp FROM inner_t WHERE grp=1)` | error 1242 / `21000` |
| `SELECT id FROM outer_t WHERE val IN (SELECT val FROM inner_t ORDER BY val LIMIT 1)` | error 1235 / `42000` |
| `SELECT id FROM outer_t WHERE (val,grp) IN (SELECT a,b FROM pair_t LIMIT 1)` | error 1235 / `42000` |
| `SELECT id FROM outer_t o WHERE EXISTS (SELECT 1 FROM inner_t WHERE missing_col=o.id)` | error 1054 / `42S22` |
| joined outer query with ambiguous unqualified outer reference | error 1052 / `23000` |

### Metadata Tests

Verify with the public result descriptor API:

| SQL | Expected metadata |
| --- | --- |
| `SELECT (SELECT txt FROM inner_t WHERE id=101) AS scalar_txt LIMIT 0` | label `scalar_txt`; no origin schema/table/column; string type and length inherited from subquery expression; nullable |
| `SELECT (SELECT val FROM inner_t WHERE id=101) + 1 AS scalar_plus LIMIT 0` | numeric expression metadata; no origin |
| `SELECT EXISTS (SELECT NULL FROM inner_t WHERE val IS NULL) AS exists_result LIMIT 0` | integer boolean metadata; length `1`; no origin |
| `SELECT val IN (SELECT val FROM inner_t WHERE grp=1) AS in_result FROM outer_t LIMIT 0` | integer boolean metadata; length `1`; no origin |
| `SELECT (val,grp) IN (SELECT a,b FROM pair_t) AS row_in_result FROM outer_t LIMIT 0` | integer boolean metadata; length `1`; no origin |

### Interaction Tests

Add focused tests for each currently supported outer surface:

- projection scalar subquery over an uncorrelated table
- projection correlated scalar subquery
- `WHERE EXISTS` over an inner join outer query
- `LEFT JOIN ... ON EXISTS (...)` with an outer reference to both join operands
- grouped query with scalar subquery in `HAVING`
- grouped query with scalar subquery in `ORDER BY`
- `SELECT DISTINCT` over a projected scalar subquery that collapses duplicates
- `SELECT DISTINCT` hidden order subquery that references a non-selected base
  column and must fail consistently with Task 28 error 3065
- scalar subquery with inner `ORDER BY ... LIMIT 1`
- `IN` subquery with inner `LIMIT` rejected with 1235

## Compatibility Status

Task 29 has the first executable slice implemented. MyLite executes
uncorrelated scalar subqueries, uncorrelated `EXISTS` / `NOT EXISTS`
subqueries, and uncorrelated scalar `IN` / `NOT IN` subqueries in no-table
scalar `SELECT` and the currently supported table-backed `SELECT` expression
contexts: projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY`. The
focused scalar `ANY` / `SOME` / `ALL` quantified-comparison slice has a start
spec in `docs/specs/quantified-subquery-comparisons/specs.md`, but runtime
behavior remains deferred until implementation lands.

This slice includes empty scalar-subquery results as `NULL`, one-row value
return, scalar error 1241 for multi-column operands, scalar error 1242 for
multi-row operands, scalar `IN` / `NOT IN` error 1235 for inner `LIMIT`,
warning propagation, first-slice result descriptors, and `EXISTS` checks that
do not evaluate the subquery select list.

The remaining Task 29 surfaces are deferred: correlated subqueries, row
subqueries, row `IN` / `NOT IN`, quantified `ANY` / `SOME` / `ALL` execution,
DML subquery execution, derived-table row sources, CTE/set-operation
subqueries, and optimizer behavior.
