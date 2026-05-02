# Row Quantified Subquery Comparisons

## Scope

This specification starts the Task 29 row quantified subquery comparison
follow-up. It covers the narrow MySQL 8.4.9 surface where a multi-element row
constructor appears on the left of `ANY`, `SOME`, or `ALL`.

The implementation slice should support only the row quantified forms that
MySQL accepts as row membership aliases:

- `(expr, expr, ...) = ANY (SELECT ...)`
- `(expr, expr, ...) = SOME (SELECT ...)`
- `ROW(expr, expr, ...) = ANY (SELECT ...)`
- `ROW(expr, expr, ...) = SOME (SELECT ...)`
- `(expr, expr, ...) <> ALL (SELECT ...)`
- `(expr, expr, ...) != ALL (SELECT ...)`
- `ROW(expr, expr, ...) <> ALL (SELECT ...)`
- `ROW(expr, expr, ...) != ALL (SELECT ...)`

The first executable slice should support:

- multi-element row constructors written as `(expr, expr, ...)` or
  `ROW(expr, expr, ...)`
- uncorrelated parenthesized `SELECT` subqueries whose visible output column
  count matches the left tuple width
- the current no-table scalar `SELECT` surface
- the current table-backed `SELECT` projection, `WHERE`, join `ON`, grouped
  `HAVING`, and hidden or visible `ORDER BY` expression contexts
- inner subquery row sources and clauses already supported by MyLite's
  implemented `SELECT` runtime, including table scans, joins, grouping,
  `HAVING`, `DISTINCT`, and `ORDER BY`
- MySQL-compatible result rows, errors, warnings, and boolean metadata

The first executable slice should not support:

- row `= ALL`, row `<> ANY`, row `<> SOME`, row `!= ANY`, row `!= SOME`, or
  ordered row quantified comparisons such as row `> ANY` and row `<= ALL`;
  MySQL 8.4.9 rejects these shapes with scalar quantified-comparison column
  count diagnostics
- null-safe quantified comparisons such as row `<=> ANY`; MySQL rejects the
  syntax
- one-element `ROW(expr)` row constructors in subquery contexts; MySQL rejects
  these syntactically
- correlated row quantified subqueries
- row quantified subqueries in `INSERT`, `UPDATE`, `DELETE`, `SET`, `DO`,
  stored programs, generated columns, default expressions, check constraints,
  views, or triggers
- `TABLE` and `VALUES` subqueries
- CTEs and set operations inside subqueries
- optimizer semijoin, antijoin, tuple-index lookup, materialization, and
  decorrelation behavior beyond externally visible compatibility

This is a specified and planned slice, not an implemented feature. Until the
runtime is added, MyLite should continue to return its current deterministic
unsupported/deferred diagnostics for row quantified operands rather than
executing partial behavior.

## Sources

- MySQL 8.4 Reference Manual, Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subqueries.html
- MySQL 8.4 Reference Manual, Comparisons Using Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/comparisons-using-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ANY`, `IN`, or `SOME`:
  https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ALL`:
  https://dev.mysql.com/doc/refman/8.4/en/all-subqueries.html
- MySQL 8.4 Reference Manual, Row Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/row-subqueries.html
- MySQL 8.4 Reference Manual, Subquery Errors:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html
- MySQL 8.4 Reference Manual, Restrictions on Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- Existing MyLite specs:
  - `docs/specs/subqueries/specs.md`
  - `docs/specs/quantified-subquery-comparisons/specs.md`
  - `docs/specs/row-subquery-predicates/specs.md`
  - `docs/specs/subquery-in-predicates/specs.md`
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
DROP DATABASE IF EXISTS mylite_row_quant_probe;
CREATE DATABASE mylite_row_quant_probe
  CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE mylite_row_quant_probe;

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

### Accepted Row Quantified Forms

MySQL 8.4.9 accepts row `= ANY`, row `= SOME`, row `<> ALL`, and row `!= ALL`
for multi-column subqueries. The accepted forms behave as aliases for row
`IN` and row `NOT IN`:

- row `= ANY` is row `IN`
- row `= SOME` is row `IN`
- row `<> ALL` is row `NOT IN`
- row `!= ALL` is row `NOT IN`

Verified results:

| SQL | Expected result |
| --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) = SOME (SELECT a,b FROM pair_t)` | `1` |
| `SELECT ROW(10,1) = ANY (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) <> ALL (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (10,1) != ALL (SELECT a,b FROM pair_t)` | `0` |
| `SELECT ROW(10,1) <> ALL (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (7,9) <> ALL (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (NULL,2) = ANY (SELECT a,b FROM pair_t WHERE a=999)` | `0` |
| `SELECT (NULL,2) <> ALL (SELECT a,b FROM pair_t WHERE a=999)` | `1` |

The empty-subquery cases use quantified-membership identity values, not row
scalar-subquery empty-result semantics. Empty row `= ANY` / `= SOME` returns
`0`; empty row `<> ALL` / `!= ALL` returns `1`, even when the left tuple
contains `NULL`.

### NULL and Unknown Truth Semantics

Accepted row quantified forms use the same three-valued membership semantics
as row `IN` / `NOT IN`. Candidate tuple comparisons are evaluated left to
right. A decisive earlier element can determine the candidate tuple result;
otherwise `NULL` elements can make membership unknown.

Verified results:

| SQL | Expected result |
| --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (7,2) = ANY (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (7,9) = ANY (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (NULL,2) = ANY (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (10,1) <> ALL (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (7,2) <> ALL (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (7,9) <> ALL (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (NULL,2) <> ALL (SELECT a,b FROM pair_t)` | `NULL` |

In `WHERE`, join `ON`, and `HAVING`, `NULL` predicate results filter the row
out in the same way as other MySQL three-valued predicates.

### Rejected Row Quantified Forms

MySQL 8.4.9 does not implement a general row quantified comparison surface.
The accepted alias forms above are special membership cases. Other row
operands with `ANY`, `SOME`, or `ALL` are rejected before execution as scalar
quantified comparisons with the wrong number of subquery columns.

Verified diagnostics:

| SQL | Expected behavior |
| --- | --- |
| `SELECT (10,1) = ALL (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (10,1) <> ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (10,1) <> SOME (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (10,1) != ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (10,1) > ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (10,1) >= ALL (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (10,1) < SOME (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT (10,1) <=> ANY (SELECT a,b FROM pair_t)` | syntax error 1064 / `42000` |

The implementation must not generalize row lexicographic comparison to
`ANY` / `SOME` / `ALL`. MySQL accepts row lexicographic operators for row
scalar subqueries, but not for multi-row quantified row subqueries.

### Tuple Width and Arity Diagnostics

Accepted row quantified aliases require the subquery output width to match
the left row constructor width.

Verified diagnostics:

| SQL | Expected behavior |
| --- | --- |
| `SELECT (1,2) = ANY (SELECT a FROM pair_t)` | error 1241 / `21000`, `Operand should contain 2 column(s)` |
| `SELECT (1,2) = ANY (SELECT a,b,label FROM pair_t)` | error 1241 / `21000`, `Operand should contain 2 column(s)` |
| `SELECT (1) = ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000`, `Operand should contain 1 column(s)` |
| `SELECT ROW(1) = ANY (SELECT a FROM pair_t)` | syntax error 1064 / `42000` |

`(expr)` is a scalar parenthesized expression, not a one-element row
constructor. `ROW(expr)` is not valid in this subquery shape.

### LIMIT and Cardinality

MySQL rejects `LIMIT` inside subqueries consumed by `IN`, `NOT IN`, `ANY`,
`SOME`, and `ALL`. The row alias forms preserve that restriction.

Verified diagnostics:

| SQL | Expected behavior |
| --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` |
| `SELECT (10,1) <> ALL (SELECT a,b FROM pair_t LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` |

The accepted row quantified aliases consume table subqueries and allow any
number of rows. They must not raise row scalar-subquery cardinality error 1242
when the subquery returns multiple rows. This differs from row scalar
subqueries:

| SQL | Expected behavior |
| --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) = (SELECT a,b FROM pair_t)` | error 1242 / `21000`, `Subquery returns more than 1 row` |
| `SELECT (10,1) = (SELECT a,b FROM pair_t WHERE a=999)` | `NULL` |

### Supported SELECT Contexts

The accepted aliases should work wherever the current row `IN` / `NOT IN`
runtime works.

Verified table-backed projection:

| SQL | Expected result |
| --- | --- |
| `SELECT id, (val,grp) = ANY (SELECT a,b FROM pair_t), (val,grp) <> ALL (SELECT a,b FROM pair_t) FROM outer_t ORDER BY id` | `(1,1,0)`, `(2,1,0)`, `(3,NULL,NULL)`, `(4,NULL,NULL)`, `(5,1,0)`, `(6,0,1)`, `(7,NULL,NULL)`, `(8,NULL,NULL)` |

Verified `WHERE` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t WHERE (val,grp) = ANY (SELECT a,b FROM pair_t) ORDER BY id` | `1`, `2`, `5` |
| `SELECT id FROM outer_t WHERE (val,grp) <> ALL (SELECT a,b FROM pair_t) ORDER BY id` | `6` |

Verified join `ON` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT o.id, j.id FROM outer_t AS o JOIN join_t AS j ON j.outer_id=o.id AND (j.marker_a,j.marker_b) = ANY (SELECT a,b FROM pair_t) ORDER BY o.id, j.id` | `(1,201)`, `(2,202)`, `(5,205)` |

Verified `HAVING` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT grp, COUNT(*) FROM outer_t GROUP BY grp HAVING (grp, COUNT(*)) = ANY (SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b) ORDER BY grp` | `(1,2)`, `(3,1)` |

Verified hidden `ORDER BY` expression:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t ORDER BY (val,grp) = ANY (SELECT a,b FROM pair_t) DESC, id` | `1`, `2`, `5`, `6`, `3`, `4`, `7`, `8` |

Verified no-table scalar `SELECT`:

| SQL | Expected result |
| --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t), (10,1) <> ALL (SELECT a,b FROM pair_t)` | `(1,0)` |

### Inner Subquery Clauses

`ORDER BY`, `DISTINCT`, grouping, and `HAVING` inside an accepted row
quantified alias subquery are valid when the subquery has no `LIMIT` and
returns the correct tuple width.

Verified results:

| SQL | Expected result | Warnings |
| --- | --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t ORDER BY label DESC)` | `1` | `0` |
| `SELECT (10,1) = ANY (SELECT DISTINCT a,b FROM pair_t)` | `1` | `0` |
| `SELECT (1,2) = ANY (SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b HAVING COUNT(*) >= 1)` | `1` | `0` |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t ORDER BY 1/0)` | `1` | `0` |

As with row `IN`, an inner `ORDER BY` without `LIMIT` is not observable as a
result ordering contract. MyLite may use the current subquery execution order
as long as results, warnings, and errors stay MySQL-compatible.

### Type Conversion and Warnings

Comparison warnings belong to the outer statement. Accepted row quantified
aliases preserve row `IN` / `NOT IN` warning behavior: tuple elements are
compared left to right, later elements are skipped when an earlier element is
decisive, and `= ANY` may stop scanning after a true candidate tuple.

Verified warning behavior:

| SQL | Result | Warning count | Warning rows |
| --- | --- | --- | --- |
| `SELECT (1,2) = ANY (SELECT x,y FROM warn_pair_t)` | `1` | `2` | warnings 1292 for `'1x'` and `'2x'` |
| `SELECT (1,3) = ANY (SELECT x,y FROM warn_pair_t)` | `0` | `3` | warnings 1292 for `'1x'`, `'2x'`, and `'bad'` |
| `SELECT (2,3) = ANY (SELECT x,y FROM warn_pair_t)` | `0` | `1` | warning 1292 for `'1x'`; later tuple elements are skipped after a false first element |
| `SELECT (1,3) <> ALL (SELECT x,y FROM warn_pair_t)` | `1` | `3` | same warning rows as the corresponding `= ANY` false probe |
| `SELECT (val,grp) = ANY (SELECT x,y FROM warn_pair_t) FROM outer_t ORDER BY id` | `0`, `0`, `NULL`, `0`, `0`, `0`, `0`, `0` | `8` | warning order follows per-row candidate comparison |

The first implementation should reuse the row `IN` comparison path for
accepted aliases so conversion warning counts and ordering stay aligned.

### Metadata

Accepted row quantified aliases expose MySQL integer boolean metadata:

- field type `LONGLONG`
- binary numeric collation, collation id 63
- declared length `1`
- decimals `0`
- `BINARY` and `NUM` flags
- no origin database, table, original table, or original column
- nullable result metadata; the `NOT_NULL` flag is not set because the result
  can be `NULL`

Observed metadata from `mysql --column-type-info -vvv`:

| SQL | Field | Type | Length | Flags |
| --- | --- | --- | --- | --- |
| `SELECT (val,grp) = ANY (SELECT a,b FROM pair_t) AS row_eq_any FROM outer_t LIMIT 0` | `row_eq_any` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT (val,grp) = SOME (SELECT a,b FROM pair_t) AS row_eq_some FROM outer_t LIMIT 0` | `row_eq_some` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT (val,grp) <> ALL (SELECT a,b FROM pair_t) AS row_ne_all FROM outer_t LIMIT 0` | `row_ne_all` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT (val,grp) != ALL (SELECT a,b FROM pair_t) AS row_bang_ne_all FROM outer_t LIMIT 0` | `row_bang_ne_all` | `LONGLONG` | `1` | `BINARY NUM` |

MyLite should use the existing predicate boolean descriptor machinery and
should not expose hidden subquery columns as output metadata.

### Correlation and DML

MySQL accepts row quantified aliases in correlated subqueries and DML
predicates, but those surfaces remain deferred for this MyLite slice because
they require broader nested query-block binding, per-row subquery execution,
statement rollback, affected-row accounting, warning lifetimes, and
self-reference diagnostics.

Verified MySQL behavior:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t AS o WHERE (o.val,o.grp) = ANY (SELECT p.a,p.b FROM pair_t AS p WHERE p.b=o.grp) ORDER BY id` | `1`, `2`, `5` |
| `SELECT id FROM outer_t AS o WHERE (o.val,o.grp) <> ALL (SELECT p.a,p.b FROM pair_t AS p WHERE p.b=o.grp) ORDER BY id` | `4`, `6`, `8` |
| `UPDATE dml_t SET txt='changed' WHERE (val,grp) = ANY (SELECT a,b FROM pair_t)` | `ROW_COUNT()` is `3` for the fixture |
| `UPDATE dml_t SET txt='changed' WHERE (val,grp) <> ALL (SELECT a,b FROM pair_t)` | `ROW_COUNT()` is `1` for the fixture |

Until those broader surfaces are implemented, MyLite should return its current
unsupported/deferred diagnostics instead of executing row quantified aliases
inside DML or correlated subqueries.

## Existing MyLite State

The parser already has a `MYLITE_SQL_AST_QUANTIFIED_COMPARISON` node with the
left expression, comparison operator, quantifier, and inner `SELECT`. It also
has `MYLITE_SQL_AST_ROW_CONSTRUCTOR` for multi-element `(a,b)` and
`ROW(a,b)` constructors.

The current runtime implements:

- scalar `ANY` / `SOME` / `ALL` quantified subquery comparisons
- row scalar subquery comparisons
- row `IN` / `NOT IN` subqueries

Row operands on quantified-comparison nodes are currently deferred. The
implementation for this slice should bridge only the MySQL-accepted alias
forms to the row `IN` / `NOT IN` machinery. It should leave scalar quantified
comparisons and row scalar comparisons unchanged.

## Parser and AST Design

No broad grammar expansion is required. The current grammar shape is already
able to build a quantified-comparison AST whose left child can be a row
constructor:

- child 0: left expression, possibly `MYLITE_SQL_AST_ROW_CONSTRUCTOR`
- child 1: inner `SELECT` statement
- `operator_kind`: comparison operator
- `subquery_quantifier`: `ANY`, `SOME`, or `ALL`

The implementation should classify the row-left quantified node during
analysis:

- `operator =` with `ANY` or `SOME`: route as row membership
- `operator <>` or `!=` with `ALL`: route as row non-membership
- null-safe `<=>` with any quantifier: keep MySQL-compatible syntax rejection
- every other row-left quantified combination: reject with error 1241 and
  expected column count `1`

### Lemon-Style Grammar Snippets

These snippets describe MyLite's intended grammar shape. They are
independently authored and are not copied from MySQL grammar.

```lemon
comparison_expression(A) ::= comparison_expression(B) quantified_comparison_operator(C)
        subquery_quantifier(D) subquery(E). {
    A = mylite_sql_parser_make_quantified_comparison(state, B, C.token, C.operator_kind, D, E);
}

quantified_comparison_operator(A) ::= EQ(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_EQUAL,
    };
}

quantified_comparison_operator(A) ::= NE(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
    };
}

quantified_comparison_operator(A) ::= LT(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LESS,
    };
}

quantified_comparison_operator(A) ::= LE(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
    };
}

quantified_comparison_operator(A) ::= GT(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_GREATER,
    };
}

quantified_comparison_operator(A) ::= GE(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
    };
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

The quantified operator nonterminal intentionally excludes `NULL_SAFE_EQ`.
If a later parser shape accepts `<=> ANY`, analysis must still reject it before
execution with MySQL-compatible syntax behavior.

The accepted row quantified forms do not need distinct AST node kinds. A
planner/analyzer classification flag is sufficient if it can keep metadata,
diagnostics, and runtime behavior explicit.

## Runtime Design

### Analysis

Before execution, validation should:

1. Resolve the quantified node's left expression.
2. Detect whether the unwrapped left expression is a multi-element row
   constructor.
3. For row left operands, classify only `= ANY`, `= SOME`, `<> ALL`, and
   `!= ALL` as supported alias forms.
4. Reject every other row-left quantified operator/quantifier pair with the
   same 1241 diagnostic MySQL uses for scalar quantified comparisons over a
   multi-column subquery, unless the parser already rejected the syntax.
5. Resolve each left tuple element in the current outer expression context.
6. Resolve the subquery as an independent query block.
7. Reject outer references for this first executable slice.
8. Require the visible subquery output column count to match the left tuple
   width for accepted alias forms. Raise 1241 / `21000` with the left tuple
   width otherwise.
9. Reject `LIMIT` inside accepted alias subqueries with error 1235 / `42000`.
10. Build nullable MySQL integer boolean metadata with no origin fields.

Wrong-width diagnostics should use the consumer's expected width:

- row width mismatch in an accepted alias uses the row tuple width
- scalar `(expr)` against a two-column subquery uses expected width `1`
- rejected non-alias row quantified shapes use the scalar quantified expected
  width `1`

### Evaluation

Accepted aliases should be evaluated by the row membership runtime:

1. Evaluate the left tuple once for the current outer row.
2. Execute the uncorrelated subquery in its own query block.
3. For row `= ANY` and `= SOME`, compare the left tuple with each subquery
   tuple as row equality membership.
4. For row `<> ALL` and `!= ALL`, compare membership first, then return the
   row `NOT IN` result.
5. Stop scanning `= ANY` / `= SOME` after a true candidate tuple.
6. Stop scanning `<> ALL` / `!= ALL` only when a true membership match proves
   the negated result false; otherwise track unknown candidates.
7. Return `NULL` only when no decisive true or false identity result applies
   and at least one candidate comparison was unknown.
8. Apply empty-subquery identity before left-value nullability changes the
   outcome: empty row `= ANY` / `= SOME` returns `0`; empty row `<> ALL` /
   `!= ALL` returns `1`.

The implementation should not share the scalar quantified comparison evaluator
for row-left accepted aliases, because the scalar evaluator assumes one-column
subquery rows and scalar comparison semantics.

### Warning Order

Warning behavior is observable. The implementation should preserve the current
row `IN` warning order:

- compare tuple elements left to right
- skip later tuple elements once a candidate row is known false or true
- stop `= ANY` / `= SOME` after the first true membership row
- continue `<> ALL` / `!= ALL` until a true membership row proves false or the
  subquery is exhausted

A future materialized implementation may cache an uncorrelated subquery, but it
must replay comparisons in MySQL-compatible order and preserve warning counts.

### Storage and Performance

This feature needs no persistent storage and no `.mylite` file-format change.

The first implementation may reuse the existing row `IN` / `NOT IN` nested-loop
scan. That is adequate for correctness and warning fidelity. Later
optimizations can introduce tuple materialization or lookup structures only
after tests cover externally visible result, warning, error, and metadata
behavior.

## Test Plan

### Parser Tests

Parser and analyzer coverage should include:

| SQL | Expected parser/analyzer outcome |
| --- | --- |
| `SELECT (a,b) = ANY (SELECT x,y FROM t);` | quantified-comparison AST with row constructor left, `EQUAL`, `ANY` |
| `SELECT (a,b) = SOME (SELECT x,y FROM t);` | quantified-comparison AST with row constructor left, `EQUAL`, `SOME` |
| `SELECT (a,b) <> ALL (SELECT x,y FROM t);` | quantified-comparison AST with row constructor left, `NOT_EQUAL`, `ALL` |
| `SELECT (a,b) != ALL (SELECT x,y FROM t);` | quantified-comparison AST with row constructor left, `NOT_EQUAL`, `ALL` |
| `SELECT ROW(a,b) = ANY (SELECT x,y FROM t);` | row constructor keyword form accepted |
| `SELECT ROW(a) = ANY (SELECT x FROM t);` | MySQL-compatible syntax error |
| `SELECT (a,b) <=> ANY (SELECT x,y FROM t);` | MySQL-compatible syntax error or pre-execution rejection |

### Runtime Result Tests

Runtime result tests should use the fixture above and assert:

| SQL | Expected result |
| --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) = SOME (SELECT a,b FROM pair_t)` | `1` |
| `SELECT ROW(10,1) = ANY (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (10,1) <> ALL (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (10,1) != ALL (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (7,2) = ANY (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (7,9) = ANY (SELECT a,b FROM pair_t)` | `0` |
| `SELECT (7,2) <> ALL (SELECT a,b FROM pair_t)` | `NULL` |
| `SELECT (7,9) <> ALL (SELECT a,b FROM pair_t)` | `1` |
| `SELECT (NULL,2) = ANY (SELECT a,b FROM pair_t WHERE a=999)` | `0` |
| `SELECT (NULL,2) <> ALL (SELECT a,b FROM pair_t WHERE a=999)` | `1` |

Context tests should cover:

| SQL | Expected result |
| --- | --- |
| `SELECT id, (val,grp) = ANY (SELECT a,b FROM pair_t), (val,grp) <> ALL (SELECT a,b FROM pair_t) FROM outer_t ORDER BY id` | `(1,1,0)`, `(2,1,0)`, `(3,NULL,NULL)`, `(4,NULL,NULL)`, `(5,1,0)`, `(6,0,1)`, `(7,NULL,NULL)`, `(8,NULL,NULL)` |
| `SELECT id FROM outer_t WHERE (val,grp) = ANY (SELECT a,b FROM pair_t) ORDER BY id` | `1`, `2`, `5` |
| `SELECT id FROM outer_t WHERE (val,grp) <> ALL (SELECT a,b FROM pair_t) ORDER BY id` | `6` |
| `SELECT o.id, j.id FROM outer_t AS o JOIN join_t AS j ON j.outer_id=o.id AND (j.marker_a,j.marker_b) = ANY (SELECT a,b FROM pair_t) ORDER BY o.id, j.id` | `(1,201)`, `(2,202)`, `(5,205)` |
| `SELECT grp, COUNT(*) FROM outer_t GROUP BY grp HAVING (grp, COUNT(*)) = ANY (SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b) ORDER BY grp` | `(1,2)`, `(3,1)` |
| `SELECT id FROM outer_t ORDER BY (val,grp) = ANY (SELECT a,b FROM pair_t) DESC, id` | `1`, `2`, `5`, `6`, `3`, `4`, `7`, `8` |

Inner-clause tests should cover:

| SQL | Expected result | Warnings |
| --- | --- | --- |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t ORDER BY label DESC)` | `1` | `0` |
| `SELECT (10,1) = ANY (SELECT DISTINCT a,b FROM pair_t)` | `1` | `0` |
| `SELECT (1,2) = ANY (SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b HAVING COUNT(*) >= 1)` | `1` | `0` |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t ORDER BY 1/0)` | `1` | `0` |

### Runtime Warning Tests

Runtime warning tests should compare result rows, warning counts, warning
codes, and warning messages:

| SQL | Expected rows | Expected warnings |
| --- | --- | --- |
| `SELECT (1,2) = ANY (SELECT x,y FROM warn_pair_t)` | `1` | two 1292 warnings: `'1x'`, `'2x'` |
| `SELECT (1,3) = ANY (SELECT x,y FROM warn_pair_t)` | `0` | three 1292 warnings: `'1x'`, `'2x'`, `'bad'` |
| `SELECT (2,3) = ANY (SELECT x,y FROM warn_pair_t)` | `0` | one 1292 warning: `'1x'` |
| `SELECT (1,3) <> ALL (SELECT x,y FROM warn_pair_t)` | `1` | three 1292 warnings: `'1x'`, `'2x'`, `'bad'` |
| `SELECT (val,grp) = ANY (SELECT x,y FROM warn_pair_t) FROM outer_t ORDER BY id` | `0`, `0`, `NULL`, `0`, `0`, `0`, `0`, `0` | eight 1292 warnings in MySQL-observed order |

### Runtime Diagnostic Tests

Runtime diagnostic tests should compare error code, SQLSTATE, message, and
`SHOW WARNINGS` behavior:

| SQL | Expected behavior |
| --- | --- |
| `SELECT (1,2) = ANY (SELECT a FROM pair_t)` | error 1241 / `21000`, expected width `2` |
| `SELECT (1,2) = ANY (SELECT a,b,label FROM pair_t)` | error 1241 / `21000`, expected width `2` |
| `SELECT (1) = ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000`, expected width `1` |
| `SELECT ROW(1) = ANY (SELECT a FROM pair_t)` | syntax error 1064 / `42000` |
| `SELECT (10,1) <=> ANY (SELECT a,b FROM pair_t)` | syntax error 1064 / `42000` |
| `SELECT (10,1) = ALL (SELECT a,b FROM pair_t)` | error 1241 / `21000`, expected width `1` |
| `SELECT (10,1) <> ANY (SELECT a,b FROM pair_t)` | error 1241 / `21000`, expected width `1` |
| `SELECT (10,1) = ANY (SELECT a,b FROM pair_t LIMIT 1)` | error 1235 / `42000` |
| `SELECT (10,1) <> ALL (SELECT a,b FROM pair_t LIMIT 1)` | error 1235 / `42000` |
| `SELECT (10,1) = (SELECT a,b FROM pair_t)` | row scalar comparison still raises error 1242 / `21000` |
| `SELECT id FROM outer_t WHERE (val,grp) = ANY (SELECT a,b FROM pair_t WHERE b=outer_t.grp)` | deferred correlated subquery diagnostic in the first slice |
| `UPDATE outer_t SET txt='changed' WHERE (val,grp) = ANY (SELECT a,b FROM pair_t)` | deferred DML subquery diagnostic in the first slice |

### Metadata Tests

Metadata tests should use the public result descriptor API and assert:

| SQL | Expected metadata |
| --- | --- |
| `SELECT (val,grp) = ANY (SELECT a,b FROM pair_t) AS row_eq_any FROM outer_t LIMIT 0` | label `row_eq_any`; no origin fields; `LONGLONG`; length `1`; decimals `0`; charset id `63`; nullable; `BINARY` and `NUM` flags |
| `SELECT (val,grp) = SOME (SELECT a,b FROM pair_t) AS row_eq_some FROM outer_t LIMIT 0` | same boolean metadata under label `row_eq_some` |
| `SELECT (val,grp) <> ALL (SELECT a,b FROM pair_t) AS row_ne_all FROM outer_t LIMIT 0` | same boolean metadata under label `row_ne_all` |
| `SELECT (val,grp) != ALL (SELECT a,b FROM pair_t) AS row_bang_ne_all FROM outer_t LIMIT 0` | same boolean metadata under label `row_bang_ne_all` |

### Regression Boundaries

The implementation PR should also assert that existing Task 29 behavior still
passes:

- row `IN` and row `NOT IN` truth tables
- row scalar subquery equality, ordering, `<=>`, empty-result, and multi-row
  cardinality behavior
- scalar quantified comparisons with one-column subqueries
- scalar quantified comparisons rejecting multi-column subqueries with
  expected width `1`
- scalar and row `IN` / `NOT IN` rejecting inner `LIMIT` with error 1235
- hidden `ORDER BY` subquery expressions preserving visible metadata
- `DISTINCT` hidden `ORDER BY` validation when row constructors reference
  non-selected columns

## Compatibility Status

This slice is specified and planned, not implemented.

Target implementation coverage is uncorrelated row `= ANY`, row `= SOME`, row
`<> ALL`, and row `!= ALL` aliases in no-table scalar `SELECT` and the current
table-backed `SELECT` projection, `WHERE`, join `ON`, grouped `HAVING`, and
`ORDER BY` expression contexts.

Rejected MySQL shapes, including row `= ALL`, row `<> ANY` / `<> SOME`, row
ordered quantified comparisons, and row `<=> ANY`, must keep MySQL-compatible
diagnostics. Correlation, DML contexts, derived row sources, `TABLE` /
`VALUES`, CTEs, set operations, and optimizer-specific behavior remain
separate features.
