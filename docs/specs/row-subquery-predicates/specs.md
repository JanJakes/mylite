# Row Subquery Predicates

## Scope

This specification covers the implemented Task 29 row-subquery predicate slice:
row scalar subquery comparisons and row `IN` / `NOT IN` predicates for the
first uncorrelated runtime surface.

The executable row-subquery slice supports:

- multi-element row constructors written as `(expr, expr, ...)` and
  `ROW(expr, expr, ...)`
- row comparisons where the right side is a scalar row subquery that returns
  one row with the same tuple width as the left row constructor
- row comparison operators `=`, `<>`, `!=`, `<`, `<=`, `>`, `>=`, and `<=>`
  where MySQL accepts the syntax and runtime behavior is verified
- row `IN` and row `NOT IN` predicates whose right side is a parenthesized
  `SELECT` returning the same number of columns as the left row constructor
- uncorrelated subqueries in the first runtime slice
- no-table scalar `SELECT`
- table-backed `SELECT` projection expressions
- table-backed `SELECT WHERE`
- explicit join `ON`
- grouped `HAVING`
- hidden and visible `ORDER BY` expressions
- inner subquery row sources and clauses already supported by MyLite's current
  `SELECT` implementation, including table scans, joins, grouping, `HAVING`,
  `DISTINCT`, and `ORDER BY`

The first runtime slice does not support:

- one-element `ROW(expr)` row constructors in subquery comparison contexts;
  MySQL rejects these forms syntactically
- correlated row subqueries
- row subqueries in `INSERT`, `UPDATE`, `DELETE`, `SET`, `DO`, stored-program,
  generated-column, check-constraint, default-expression, view, or trigger
  contexts
- `TABLE` and `VALUES` subqueries
- CTEs and set operations inside subqueries
- row quantified comparisons except where a later implementation deliberately
  treats row `= ANY` and row `= SOME` as row `IN` aliases after separate tests
- optimizer semijoin, antijoin, decorrelation, tuple-index lookup, or
  materialization planning beyond externally visible MySQL-compatible behavior

MySQL rejects `LIMIT` in subqueries consumed by `IN`, `NOT IN`, `ANY`, `SOME`,
and `ALL`. Row `IN` / `NOT IN` must raise error 1235 for that shape. Row
scalar comparisons such as `(a,b) = (SELECT x,y ... LIMIT 1)` may use `LIMIT`
because the subquery is a scalar row subquery, not an `IN` table subquery.

## Sources

- MySQL 8.4 Reference Manual, Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subqueries.html
- MySQL 8.4 Reference Manual, Comparisons Using Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/comparisons-using-subqueries.html
- MySQL 8.4 Reference Manual, Row Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/row-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ANY`, `IN`, or `SOME`:
  https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html
- MySQL 8.4 Reference Manual, Subquery Errors:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html
- MySQL 8.4 Reference Manual, Restrictions on Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- Existing MyLite specs:
  - `docs/specs/subqueries/specs.md`
  - `docs/specs/subquery-in-predicates/specs.md`
  - `docs/specs/quantified-subquery-comparisons/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/inner-joins/specs.md`
  - `docs/specs/outer-joins/specs.md`
  - `docs/specs/aggregate-grouping/specs.md`
  - `docs/specs/order-limit-offset/specs.md`

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
DROP DATABASE IF EXISTS mylite_task29_row_subqueries;
CREATE DATABASE mylite_task29_row_subqueries
  CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE mylite_task29_row_subqueries;

CREATE TABLE outer_t (
  id INT PRIMARY KEY,
  grp INT NULL,
  val INT NULL,
  txt VARCHAR(16) NULL
);

CREATE TABLE pair_t (
  a INT NULL,
  b INT NULL,
  label VARCHAR(16) NULL
);

CREATE TABLE join_t (
  id INT PRIMARY KEY,
  outer_id INT NULL,
  marker_a INT NULL,
  marker_b INT NULL
);

CREATE TABLE warn_pair_t (
  x VARCHAR(16) NULL,
  y VARCHAR(16) NULL
);

INSERT INTO outer_t VALUES
  (1,1,10,'alpha'),
  (2,1,20,'beta'),
  (3,2,NULL,'gamma'),
  (4,NULL,5,NULL),
  (5,3,30,'delta'),
  (6,9,7,'epsilon'),
  (7,2,7,'zeta'),
  (8,NULL,10,'eta');

INSERT INTO pair_t VALUES
  (10,1,'match10'),
  (20,1,'match20'),
  (NULL,2,'null_a'),
  (30,3,'match30'),
  (5,NULL,'null_b'),
  (10,NULL,'null_b_for_10');

INSERT INTO join_t VALUES
  (201,1,10,1),
  (202,2,20,1),
  (203,3,NULL,2),
  (204,4,5,NULL),
  (205,5,30,3),
  (206,6,7,9),
  (207,7,NULL,2),
  (208,8,10,NULL);

INSERT INTO warn_pair_t VALUES
  ('1x','2x'),
  ('1','bad'),
  ('9','bad');
```

### Row Constructors

MySQL accepts multi-element row constructors written as either `(a,b)` or
`ROW(a,b)`. In subquery comparison contexts, `ROW(1)` is not a one-element row
subquery operand; MySQL rejects it with syntax error 1064. A parenthesized
single expression such as `(1)` remains a scalar expression.

The left row constructor and the subquery output row must have the same number
of values. Row `IN` and row `NOT IN` compare a left tuple against every tuple
returned by the table subquery.

### Row Comparison Semantics

Row comparisons are lexicographic for ordered comparisons and use
three-valued equality for equality comparisons. Evaluation proceeds left to
right, and a decisive earlier element can determine the result even if a later
element is `NULL`.

Verified results:

| SQL | Expected result |
| --- | --- |
| `SELECT (1,2) = (SELECT 1,2)` | `1` |
| `SELECT (1,2) = (SELECT 1,3)` | `0` |
| `SELECT (1,NULL) = (SELECT 1,NULL)` | `NULL` |
| `SELECT (NULL,2) = (SELECT NULL,2)` | `NULL` |
| `SELECT (1,2) <> (SELECT 1,3)` | `1` |
| `SELECT (1,NULL) <> (SELECT 1,NULL)` | `NULL` |
| `SELECT (1,2) < (SELECT 1,3)` | `1` |
| `SELECT (1,4) < (SELECT 2,0)` | `1` |
| `SELECT (2,0) < (SELECT 1,99)` | `0` |
| `SELECT (1,NULL) < (SELECT 1,3)` | `NULL` |
| `SELECT (0,NULL) < (SELECT 1,3)` | `1` |
| `SELECT (2,NULL) < (SELECT 1,3)` | `0` |
| `SELECT ROW(1,2) = (SELECT 1,2)` | `1` |
| `SELECT (1,2) <=> (SELECT 1,2)` | `1` |
| `SELECT (1,2) <=> (SELECT 1,3)` | `0` |
| `SELECT (1,NULL) <=> (SELECT 1,NULL)` | `1` |
| `SELECT (NULL,2) <=> (SELECT NULL,2)` | `0` |
| `SELECT (NULL,NULL) <=> (SELECT NULL,NULL)` | `1` |
| `SELECT (1,2) <=> (SELECT a,b FROM pair_t WHERE a=999)` | `0` |

A row scalar subquery that returns no rows yields `NULL` for row comparison
operators, matching scalar subquery empty-result behavior:

| SQL | Expected result |
| --- | --- |
| `SELECT (1,2) = (SELECT a,b FROM pair_t WHERE a=999)` | `NULL` |
| `SELECT (1,2) <> (SELECT a,b FROM pair_t WHERE a=999)` | `NULL` |
| `SELECT (1,2) < (SELECT a,b FROM pair_t WHERE a=999)` | `NULL` |

Null-safe row subquery comparison has verified edge cases that differ from a
simple element-by-element rewrite over two literal row constructors. In
particular, `(NULL,2) <=> (SELECT NULL,2)` returns `0` while
`(NULL,2) <=> (NULL,2)` returns `1`. The implementation must follow
subquery-consumer behavior for row subqueries and should not infer it from
direct row-constructor comparison alone.

### Row IN and Row NOT IN Truth Semantics

Row `IN` returns `1` if any row comparison is true, `0` for an empty subquery,
`0` when every row comparison is false and none are unknown, and `NULL` when
no row comparison is true but at least one row comparison is unknown.

Row `NOT IN` is the logical negation of row membership under MySQL's
three-valued semantics. It returns `1` for an empty subquery, `0` for a true
membership match, `1` when every row comparison is false and none are unknown,
and `NULL` when membership is unknown.

Verified results:

| SQL | Expected result |
| --- | --- |
| `SELECT (10,1) IN (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) NOT IN (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (7,2) IN (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (7,2) NOT IN (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (NULL,2) IN (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (10,NULL) IN (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (7,9) IN (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (7,9) NOT IN (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) IN (SELECT a,b FROM pair_t WHERE a=999)` | `0` |
| `SELECT (10,1) NOT IN (SELECT a,b FROM pair_t WHERE a=999)` | `1` |
| `SELECT (NULL,2) IN (SELECT a,b FROM pair_t WHERE a=999)` | `0` |
| `SELECT (NULL,2) NOT IN (SELECT a,b FROM pair_t WHERE a=999)` | `1` |

The empty-subquery behavior is important: row `IN` over an empty result is
false and row `NOT IN` is true even when the left tuple contains `NULL`.

### Supported SELECT Contexts

The same row predicate semantics apply wherever this slice exposes row
subquery predicates.

Verified table-backed projection:

| SQL | Expected result |
| --- | --- |
| `SELECT id, (val,grp) IN (SELECT a,b FROM pair_t) AS in_pair, (val,grp) NOT IN (SELECT a,b FROM pair_t) AS not_in_pair FROM outer_t ORDER BY id` | `(1,1,0)`, `(2,1,0)`, `(3,NULL,NULL)`, `(4,NULL,NULL)`, `(5,1,0)`, `(6,0,1)`, `(7,NULL,NULL)`, `(8,NULL,NULL)` |

Verified `WHERE` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t WHERE (val,grp) IN (SELECT a,b FROM pair_t) ORDER BY id` | `1`, `2`, `5` |

Verified correlated MySQL behavior, deferred for the first MyLite runtime
slice:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t AS o WHERE (o.val,o.grp) IN (SELECT p.a,p.b FROM pair_t AS p WHERE p.b=o.grp) ORDER BY id` | `1`, `2`, `5` |

Verified join `ON` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT o.id, j.id FROM outer_t AS o JOIN join_t AS j ON j.outer_id=o.id AND (j.marker_a,j.marker_b) IN (SELECT a,b FROM pair_t) ORDER BY o.id, j.id` | `(1,201)`, `(2,202)`, `(5,205)` |

Verified `HAVING` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT grp, COUNT(*) AS c FROM outer_t GROUP BY grp HAVING (grp, COUNT(*)) IN (SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b) ORDER BY grp` | `(1,2)`, `(3,1)` |

Verified hidden `ORDER BY` expression:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t ORDER BY (val,grp) IN (SELECT a,b FROM pair_t) DESC, id` | `1`, `2`, `5`, `6`, `3`, `4`, `7`, `8` |

Verified no-table scalar `SELECT`:

| SQL | Expected result |
| --- | --- |
| `SELECT (10,1) IN (SELECT a,b FROM pair_t) AS row_in, (10,1) NOT IN (SELECT a,b FROM pair_t) AS row_not_in` | `(1,0)` |

### Inner Subquery Clauses

`ORDER BY`, `DISTINCT`, grouping, and `HAVING` inside a row `IN` subquery are
valid when the subquery has no `LIMIT` and returns the correct tuple width.

Verified results:

| SQL | Expected result | Warnings |
| --- | --- | --- |
| `SELECT (10,1) IN (SELECT a,b FROM pair_t ORDER BY label DESC)` | `1` | `0` |
| `SELECT (10,1) IN (SELECT DISTINCT a,b FROM pair_t)` | `1` | `0` |
| `SELECT (1,2) IN (SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b HAVING COUNT(*) >= 1)` | `1` | `0` |

### Type Conversion and Warnings

Comparison warnings belong to the outer statement. Row comparisons compare
tuple elements left to right and avoid later element comparisons when an
earlier element decides that candidate row.

Verified warning behavior:

| SQL | Result | Warning count | Warning rows |
| --- | --- | --- | --- |
| `SELECT (1,2) IN (SELECT x,y FROM warn_pair_t)` | `1` | `2` | warnings 1292 for `'1x'` and `'2x'`; membership stops after the true row |
| `SELECT (1,3) IN (SELECT x,y FROM warn_pair_t)` | `0` | `3` | warnings 1292 for `'1x'`, `'2x'`, and `'bad'` |
| `SELECT (2,3) IN (SELECT x,y FROM warn_pair_t)` | `0` | `1` | warning 1292 for `'1x'`; later elements are skipped after false first elements |
| `SELECT (1,3) NOT IN (SELECT x,y FROM warn_pair_t)` | `1` | `3` | same warning rows as the corresponding `IN` probe |

The implementation must preserve warning order and counts for supported
comparison types. A future optimizer may cache or materialize an uncorrelated
subquery, but the externally visible warning behavior must remain
MySQL-compatible.

### Diagnostics

Row subquery consumers must validate tuple width and row cardinality according
to the consuming expression:

- row scalar comparisons require one output row or an empty result and the
  same number of output columns as the left row constructor
- row `IN` / `NOT IN` predicates require the same number of output columns as
  the left row constructor and allow any number of output rows
- scalar left operands still require one subquery output column

Verified diagnostics:

| SQL | Expected behavior |
| --- | --- |
| `SELECT (1,2) IN (SELECT a FROM pair_t)` | error 1241 / `21000`, `Operand should contain 2 column(s)` |
| `SELECT (1) IN (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (1,2) = (SELECT a,b FROM pair_t)` | error 1242 / `21000`, `Subquery returns more than 1 row` |
| `SELECT (1,2) = (SELECT a FROM pair_t LIMIT 1)` | error 1241 / `21000`, `Operand should contain 2 column(s)` |
| `SELECT (1,2) IN (SELECT a,b FROM pair_t LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` |
| `SELECT ROW(1) = (SELECT a FROM pair_t LIMIT 1)` | syntax error 1064 / `42000` |

Subquery-specific errors to preserve:

| Code | SQLSTATE | Meaning |
| --- | --- | --- |
| 1235 | `42000` | unsupported `LIMIT` in row `IN` / `NOT IN` subquery |
| 1241 | `21000` | subquery returns the wrong number of columns for the consuming row or scalar expression |
| 1242 | `21000` | row scalar subquery returns more than one row |
| 1064 | `42000` | syntactically invalid row constructor shape such as `ROW(1)` in this context |
| 1054 | `42S22` | unknown column while resolving the outer row or inner subquery |
| 1093 | `HY000` | DML target-table conflicts for prohibited self-referencing subqueries; deferred from this slice |

### Metadata

Row comparison, row `IN`, and row `NOT IN` predicates expose MySQL integer
boolean metadata:

- field type `LONGLONG`
- binary numeric collation, collation id 63
- declared length `1`
- decimals `0`
- `BINARY` and `NUM` flags
- no origin database, table, original table, or original column
- nullable result metadata for ordinary row comparisons, row `IN`, and row
  `NOT IN`; the `NOT_NULL` flag is not set because the result can be `NULL`
- row `<=>` scalar-subquery comparison is null-safe and exposes `NOT_NULL`
  metadata because it always returns `0` or `1`

Observed metadata from `mysql --column-type-info -vvv`:

| SQL | Field | Type | Length | Flags |
| --- | --- | --- | --- | --- |
| `SELECT (val,grp) IN (SELECT a,b FROM pair_t) AS row_in_result FROM outer_t LIMIT 0` | `row_in_result` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT (val,grp) NOT IN (SELECT a,b FROM pair_t) AS row_not_in_result FROM outer_t LIMIT 0` | `row_not_in_result` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT (val,grp) = (SELECT a,b FROM pair_t WHERE a=10 AND b=1) AS row_eq_result FROM outer_t LIMIT 0` | `row_eq_result` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT (val,grp) <=> (SELECT a,b FROM pair_t WHERE a=10 AND b=1) AS row_nse_result FROM outer_t LIMIT 0` | `row_nse_result` | `LONGLONG` | `1` | `NOT_NULL BINARY NUM` |
| `SELECT (1,2) IN (SELECT a,b FROM pair_t WHERE a=999) AS no_table_row_in LIMIT 0` | `no_table_row_in` | `LONGLONG` | `1` | `BINARY NUM` |

MyLite should use the Task 23 result descriptor machinery and should not expose
hidden subquery columns as output fields.

### DML Behavior

MySQL allows row subquery predicates in DML. This is intentionally deferred for
the first MyLite row-subquery runtime slice because correct DML support needs
statement rollback, affected rows, warnings, target-table restrictions, and
self-reference diagnostics to be tested together.

Verified MySQL behavior:

| SQL | Expected result |
| --- | --- |
| `UPDATE outer_t SET txt='changed' WHERE (val,grp) IN (SELECT a,b FROM pair_t)` | `ROW_COUNT()` is `3` for the fixture |

Until DML contexts are implemented, MyLite should keep returning its existing
unsupported-feature diagnostic for row subqueries in DML rather than executing
only part of the behavior.

### Row Quantified Interaction

Runtime probes show MySQL accepts row `= ANY` and row `= SOME` with
multi-column subqueries and produces the same result as row `IN` for the
verified cases:

| SQL | Expected result |
| --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) = SOME (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) > ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |

This row-subquery slice should focus on row scalar comparisons and row
`IN` / `NOT IN`. Row `= ANY` / `= SOME` may be implemented as a follow-up once
the row tuple comparison path is stable, or included only if the implementation
can preserve all row `IN` semantics and diagnostics without broadening support
to unsupported row quantified operators.
That follow-up is specified in
`docs/specs/row-quantified-subquery-comparisons/specs.md`.

## MyLite Parser and AST Design

The current parser already accepts the important multi-element shapes:

- `(expr, expr, ...) IN (SELECT ...)`
- `(expr, expr, ...) NOT IN (SELECT ...)`
- `ROW(expr, expr, ...) = (SELECT ...)`
- `(expr, expr, ...) = (SELECT ...)`

It also already represents row constructors as `MYLITE_SQL_AST_ROW_CONSTRUCTOR`
and subquery predicates as existing binary or subquery expression nodes. The
runtime implementation should reuse those nodes instead of adding a separate
row-subquery AST type unless tuple-specific state cannot be represented
cleanly.

Recommended child order:

- row constructor: children are tuple elements in source order
- row scalar comparison: binary comparison node, child 0 is the row
  constructor, child 1 is `MYLITE_SQL_AST_SUBQUERY_EXPRESSION`
- row `IN` / `NOT IN`: binary expression node, child 0 is the row constructor,
  child 1 is the inner `SELECT`; node operator is `IN` or `NOT_IN`

The analyzer, not the parser, should enforce tuple arity, unsupported
correlation, unsupported DML contexts, and `LIMIT` restrictions.

## Lemon-Style Grammar Snippets

These snippets describe MyLite's intended grammar shape for row subquery
predicates. They are independently authored and are not copied from MySQL
grammar.

```lemon
primary_expression(A) ::= row_constructor(B). {
    A = B;
}

primary_expression(A) ::= subquery(B). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, B);
}

comparison_expression(A) ::= comparison_expression(B) EQ(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_EQUAL, C);
}

comparison_expression(A) ::= comparison_expression(B) NE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, C);
}

comparison_expression(A) ::= comparison_expression(B) LT(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS, C);
}

comparison_expression(A) ::= comparison_expression(B) LE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, C);
}

comparison_expression(A) ::= comparison_expression(B) GT(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER, C);
}

comparison_expression(A) ::= comparison_expression(B) GE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, C);
}

comparison_expression(A) ::= comparison_expression(B) NULL_SAFE_EQ(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, C);
}

comparison_expression(A) ::= comparison_expression(B) IN(T) subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IN, C);
}

comparison_expression(A) ::= comparison_expression(B) NOT(T) IN subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_IN, C);
}

row_constructor(A) ::= LPAREN(L) expression(B) COMMA expression_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_row_constructor(
        state, L,
        (struct mylite_sql_parser_row_constructor_elements){
            .first_expression = B,
            .remaining_expressions = C,
        },
        R);
}

row_constructor(A) ::= ROW(T) LPAREN expression(B) COMMA expression_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_row_constructor(
        state, T,
        (struct mylite_sql_parser_row_constructor_elements){
            .first_expression = B,
            .remaining_expressions = C,
        },
        R);
}

subquery(A) ::= LPAREN(L) select_statement(B) RPAREN(R). {
    A = (struct mylite_sql_parser_subquery){
        .left_paren = L,
        .select_statement = B,
        .right_paren = R,
    };
}
```

One-element `ROW(expr)` is intentionally absent. A parenthesized single
expression remains an ordinary parenthesized expression:

```lemon
primary_expression(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
```

The row quantified alias surface is deferred from this slice:

```lemon
/* Deferred until row IN is stable and separately verified. */
comparison_expression ::= row_constructor EQ ANY subquery.
comparison_expression ::= row_constructor EQ SOME subquery.
```

## Runtime Design

### Analysis

Before execution, validation should:

1. Resolve every element of the left row constructor in the outer expression
   context.
2. Resolve the subquery as its own query block.
3. Reject correlated subquery references for the first runtime slice.
4. Require at least two row-constructor elements for row subquery contexts.
5. Require the visible subquery output column count to match the left tuple
   width. Raise 1241 / `21000` with the expected tuple width otherwise.
6. For row scalar comparisons, allow zero or one subquery row and raise
   1242 / `21000` if the subquery produces more than one row.
7. For row `IN` / `NOT IN`, allow any number of subquery rows.
8. Reject `LIMIT` inside row `IN` / `NOT IN` subqueries with error 1235 /
   `42000`.
9. Build nullable MySQL integer boolean metadata with no origin fields.

### Evaluation

The evaluator should compare tuple candidates left to right:

1. Evaluate the left tuple once for the current outer row.
2. For row scalar comparisons, execute the subquery and collect at most two
   rows so 1242 can be detected without materializing an unbounded result.
3. For row `IN` / `NOT IN`, stream subquery rows in MySQL-visible order.
4. Compare each tuple element using the existing scalar comparison machinery so
   conversion, collation, and warning behavior stays centralized.
5. Stop comparing elements in a candidate tuple as soon as the candidate result
   is decisive.
6. Stop scanning row `IN` as soon as a candidate tuple is true.
7. Stop scanning row `NOT IN` only when a true membership match proves the
   negated result false; otherwise track whether any candidate was unknown.
8. Return `NULL` only when no decisive true or false identity result applies
   and at least one candidate comparison was unknown.

### Storage and Performance

This feature does not require schema or file-format changes.

A simple first implementation may rescan uncorrelated subqueries per outer row
to preserve warning behavior and reduce planner complexity. Later
materialization is acceptable only if it preserves:

- row order where warning order is observable
- type conversion warnings and warning counts
- `NULL` truth semantics
- tuple arity diagnostics
- statement-owned temporary value lifetime

Future optimization can introduce tuple materialization, row hash lookup, or
index-backed semijoin/antijoin paths after the baseline semantics are covered
by MySQL-runtime comparison tests.

## Test Plan

Parser tests should cover:

- `(a,b) IN (SELECT x,y FROM t)`
- `(a,b) NOT IN (SELECT x,y FROM t)`
- `ROW(a,b) = (SELECT x,y FROM t)`
- `(a,b) < (SELECT x,y FROM t)`
- one-element `ROW(a)` syntax rejection
- scalar `(a)` with a two-column subquery producing 1241 at analysis/runtime,
  not a row-constructor parse
- row quantified aliases kept deferred or explicitly routed only for `= ANY`
  and `= SOME` if the implementation chooses to include them

Runtime tests should cover:

- row comparison equality, inequality, ordering, and `<=>` semantics
- row scalar subquery empty result as `NULL`
- row scalar subquery multi-row error 1242
- row `IN` / `NOT IN` matches, misses, empty subqueries, left `NULL`, right
  `NULL`, and unknown membership results
- projection, no-table scalar `SELECT`, `WHERE`, join `ON`, `HAVING`, and
  hidden `ORDER BY` contexts
- inner `ORDER BY`, `DISTINCT`, grouping, and `HAVING`
- error 1235 for row `IN` / `NOT IN` with inner `LIMIT`
- error 1241 for tuple width mismatch
- error 1054 for unknown outer or inner columns
- error 3065 for `DISTINCT` with a hidden row-subquery `ORDER BY` expression
  that references non-selected outer columns
- warning counts and order for numeric/string conversion in tuple elements
- nullable `LONGLONG(1)` boolean metadata with no origin fields, except
  null-safe row scalar comparison which exposes `NOT_NULL`
- unsupported diagnostics for correlated row subqueries in the first runtime
  slice
- unsupported diagnostics for DML contexts in the first runtime slice
- no regressions for scalar subqueries, `EXISTS`, scalar `IN` / `NOT IN`, and
  scalar quantified comparisons

## Remaining Implementation Risks

- Correlation should remain explicitly rejected in the first runtime slice
  until nested query-block binding and per-row evaluation are implemented.
- DML support should remain deferred until statement rollback, affected rows,
  warnings, and target-table restrictions are tested together.
- Future row quantified-comparison work must not accidentally change the row
  `IN` and row scalar subquery truth tables covered here.
