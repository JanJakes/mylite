# Quantified Subquery Comparisons

## Scope

This specification starts the next Task 29 subquery slice: executable scalar
quantified comparisons whose right side is an uncorrelated table subquery and
whose quantifier is `ANY`, `SOME`, or `ALL`.

The first executable slice should support:

- scalar left operands from the currently supported expression subset,
  including literals, column references, aggregate outputs where already legal,
  scalar functions where already legal, `CASE`, `CAST`, and deterministic scalar
  operator expressions
- comparison operators MySQL 8.4.9 permits for quantified subqueries:
  `=`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`
- parenthesized `SELECT` subqueries that return exactly one visible column and
  zero or more rows
- uncorrelated subqueries only; the subquery must not reference an outer query
  block
- no-table scalar `SELECT`
- table-backed `SELECT` projection expressions
- table-backed `SELECT WHERE`
- explicit join `ON`
- grouped `HAVING`
- hidden and visible `ORDER BY` expressions
- inner subquery row sources and clauses already supported by MyLite's current
  `SELECT` implementation, including table scans, joins, grouping, `HAVING`,
  `DISTINCT`, and `ORDER BY`

The first executable slice should not support:

- row left operands outside the accepted alias forms implemented by
  `docs/specs/row-quantified-subquery-comparisons/specs.md`
- multi-column subqueries
- correlated subquery references
- `INSERT`, `UPDATE`, `DELETE`, `SET`, `DO`, stored-program, generated-column,
  check-constraint, default-expression, view, or trigger contexts
- scalar subquery comparisons such as `expr = (SELECT ...)`; those are already
  covered by the broad subquery slice
- scalar and row `IN` / `NOT IN`; scalar forms are covered by
  `docs/specs/subquery-in-predicates/specs.md`
- derived tables, `LATERAL`, `TABLE`, or `VALUES` subqueries
- CTEs and set operations inside subqueries
- optimizer semijoin, antijoin, decorrelation, or index-driven execution

MySQL rejects `LIMIT` in subqueries consumed by `IN`, `NOT IN`, `ANY`, `SOME`,
and `ALL`. This slice must implement that validation as error 1235 rather than
silently executing the subquery with a limit.

MyLite's parser currently accepts `<=> ANY (subquery)` because quantified
comparisons reuse the generic comparison-operator nonterminal. MySQL 8.4.9
rejects that syntax. The implementation should close that parser gap or reject
the shape before execution with a MySQL-compatible syntax diagnostic. It must
not implement null-safe quantified comparison semantics unless later MySQL
runtime evidence proves a valid syntax shape.

## Sources

- MySQL 8.4 Reference Manual, Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subqueries.html
- MySQL 8.4 Reference Manual, Comparisons Using Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/comparisons-using-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ANY`, `IN`, or `SOME`:
  https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ALL`:
  https://dev.mysql.com/doc/refman/8.4/en/all-subqueries.html
- MySQL 8.4 Reference Manual, Subquery Errors:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html
- MySQL 8.4 Reference Manual, Restrictions on Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- Existing MyLite specs:
  - `docs/specs/subqueries/specs.md`
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
DROP DATABASE IF EXISTS mylite_task29_quantified;
CREATE DATABASE mylite_task29_quantified
  CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE mylite_task29_quantified;

CREATE TABLE outer_t (
  id INT PRIMARY KEY,
  grp INT NULL,
  val INT NULL,
  txt VARCHAR(16) NULL
);

CREATE TABLE set_t (
  set_name VARCHAR(16) NOT NULL,
  n INT NULL,
  txt VARCHAR(16) NULL
);

CREATE TABLE join_t (
  id INT PRIMARY KEY,
  outer_id INT NULL,
  marker INT NULL
);

INSERT INTO outer_t VALUES
  (1,1,10,'alpha'),
  (2,1,20,'beta'),
  (3,2,NULL,'gamma'),
  (4,NULL,5,NULL),
  (5,3,30,'delta');

INSERT INTO set_t VALUES
  ('base',10,'10'),
  ('base',20,'20'),
  ('base',NULL,NULL),
  ('small',1,'1'),
  ('small',5,'5'),
  ('small',NULL,NULL),
  ('order',20,'20'),
  ('order',NULL,NULL),
  ('having',2,'2'),
  ('having',3,'3'),
  ('warn',NULL,'1x'),
  ('warn',NULL,'abc');

INSERT INTO join_t VALUES
  (201,1,10),
  (202,2,20),
  (203,3,30),
  (204,4,NULL),
  (205,5,5);
```

### Quantifier Semantics

`ANY` and `SOME` are synonyms. For `left op ANY (subquery)` and
`left op SOME (subquery)`, MySQL compares the scalar left value with each value
from the one-column subquery:

- return `1` if at least one comparison is true
- return `0` if the subquery is empty
- return `0` if every comparison is false and none are unknown
- return `NULL` if no comparison is true and at least one comparison is unknown

`ALL` is universal quantification:

- return `1` if the subquery is empty
- return `1` if every comparison is true and none are unknown
- return `0` if any comparison is false
- return `NULL` if no comparison is false and at least one comparison is unknown

Verified results:

| SQL | Expected result |
| --- | --- |
| `SELECT 10 = ANY (SELECT n FROM set_t WHERE set_name='base')` | `1` |
| `SELECT 10 <> SOME (SELECT n FROM set_t WHERE set_name='base')` | `1` |
| `SELECT 10 != SOME (SELECT n FROM set_t WHERE set_name='base')` | `1` |
| `SELECT 20 >= ALL (SELECT n FROM set_t WHERE set_name='base' AND n IS NOT NULL)` | `1` |
| `SELECT 20 >= ALL (SELECT n FROM set_t WHERE set_name='base')` | `NULL` |
| `SELECT 7 > ANY (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT 7 > ALL (SELECT n FROM set_t WHERE set_name='missing')` | `1` |
| `SELECT NULL > ANY (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT NULL > ALL (SELECT n FROM set_t WHERE set_name='missing')` | `1` |
| `SELECT 7 > ANY (SELECT n FROM set_t WHERE set_name='order')` | `NULL` |
| `SELECT 7 > ALL (SELECT n FROM set_t WHERE set_name='order')` | `0` |
| `SELECT 7 < ANY (SELECT n FROM set_t WHERE set_name='small')` | `NULL` |
| `SELECT 7 < ALL (SELECT n FROM set_t WHERE set_name='small')` | `0` |
| `SELECT 10 = ALL (SELECT n FROM set_t WHERE set_name='missing')` | `1` |
| `SELECT 10 = ANY (SELECT n FROM set_t WHERE set_name='missing')` | `0` |

The empty-subquery cases are not the same as scalar subquery comparison. An
empty scalar subquery becomes a single `NULL` value before comparison; an empty
quantified subquery returns the quantifier identity value.

### Relationship to IN and NOT IN

For subqueries, `expr IN (subquery)` behaves like `expr = ANY (subquery)`.
`expr NOT IN (subquery)` behaves like `expr <> ALL (subquery)`. `NOT IN` is
not equivalent to `expr <> ANY (subquery)`.

This slice should share comparison and warning machinery with scalar
`IN` / `NOT IN` subqueries, but it should keep a distinct AST/runtime path so
all comparison operators and `ALL` identity behavior are explicit.

### Supported SELECT Contexts

The same predicate semantics apply wherever this slice exposes scalar
quantified comparisons.

Verified no-table scalar `SELECT`:

| SQL | Expected result |
| --- | --- |
| `SELECT 10 = ANY (SELECT n FROM set_t WHERE set_name='base'), 5 < ALL (SELECT n FROM set_t WHERE set_name='base' AND n IS NOT NULL)` | `(1,1)` |

Verified table-backed projection:

| SQL | Expected result |
| --- | --- |
| `SELECT id, val > ANY (SELECT n FROM set_t WHERE set_name='base') AS gt_any_base, val >= ALL (SELECT n FROM set_t WHERE set_name='base') AS ge_all_base FROM outer_t ORDER BY id` | `(1,NULL,0)`, `(2,1,NULL)`, `(3,NULL,NULL)`, `(4,NULL,0)`, `(5,1,NULL)` |

Verified `WHERE` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t WHERE val > ANY (SELECT n FROM set_t WHERE set_name='base') ORDER BY id` | `2`, `5` |
| `SELECT id FROM outer_t WHERE val >= ALL (SELECT n FROM set_t WHERE set_name='base' AND n IS NOT NULL) ORDER BY id` | `2`, `5` |

Verified join `ON` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT o.id, j.id FROM outer_t AS o JOIN join_t AS j ON j.outer_id=o.id AND j.marker = ANY (SELECT n FROM set_t WHERE set_name='base') ORDER BY o.id, j.id` | `(1,201)`, `(2,202)` |

Verified `HAVING` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT grp, COUNT(*) AS c FROM outer_t GROUP BY grp HAVING COUNT(*) = ANY (SELECT n FROM set_t WHERE set_name='having') ORDER BY grp` | `(1,2)` |

Verified hidden `ORDER BY` expression:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t ORDER BY val <> SOME (SELECT n FROM set_t WHERE set_name='order') DESC, id` | `1`, `4`, `5`, `2`, `3` |

### Inner Subquery Clauses

`ORDER BY`, `DISTINCT`, grouping, and `HAVING` inside a quantified subquery are
valid when the subquery has no `LIMIT` and returns one column.

Verified results:

| SQL | Expected result | Warnings |
| --- | --- | --- |
| `SELECT 10 > ANY (SELECT n FROM set_t WHERE set_name='base' ORDER BY n DESC)` | `NULL` | `0` |
| `SELECT 10 = ANY (SELECT DISTINCT n FROM set_t WHERE set_name IN ('base','order'))` | `1` | `0` |
| `SELECT 2 = ALL (SELECT COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) >= 2)` | `1` | `0` |
| `SELECT 10 > ANY (SELECT n FROM set_t WHERE set_name='base' ORDER BY 1/0)` | `NULL` | `0` |
| `SELECT 10 > ALL (SELECT n FROM set_t WHERE set_name='base' ORDER BY 1/0)` | `0` | `0` |

As with scalar `IN` subqueries, the inner `ORDER BY` is not a contract for
comparison order when no `LIMIT` is present. MyLite may execute the current
subquery plan order as long as result, warning, and error behavior stays covered
by compatibility tests.

### Type Conversion and Warnings

Comparison warnings belong to the outer statement. MySQL compares candidate
rows until the quantified result is determined. This makes warning counts
observable.

Verified warning behavior:

| SQL | Result | Warning count | Warning rows |
| --- | --- | --- | --- |
| `SELECT 0 > ANY (SELECT txt FROM set_t WHERE set_name='warn')` | `0` | `2` | two warnings, code 1292, for `'1x'` and `'abc'` |
| `SELECT 1 = ANY (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | `1` | one warning, code 1292, for `'1x'` |
| `SELECT 0 > ALL (SELECT txt FROM set_t WHERE set_name='warn')` | `0` | `1` | one warning, code 1292, for `'1x'` |
| `SELECT 2 > ALL (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | `2` | two warnings, code 1292, for `'1x'` and `'abc'` |
| `SELECT val > ANY (SELECT txt FROM set_t WHERE set_name='warn') FROM outer_t ORDER BY id` | `1`, `1`, `NULL`, `1`, `1` | `4` | one warning for each non-`NULL` outer `val`; no warnings for the `NULL` left operand |

The first implementation should preserve these warning counts for supported
expression types. If the comparison engine cannot yet produce a specific
conversion warning, the test should stay as an expected gap rather than
silently accepting a warning count mismatch.

### Diagnostics

A quantified subquery used with a scalar left operand must return exactly one
column. It may return zero, one, or many rows. Multi-row results are normal for
`ANY`, `SOME`, and `ALL`; they are not scalar-subquery error 1242 cases.

Verified diagnostics:

| SQL | Expected behavior | `SHOW WARNINGS` |
| --- | --- | --- |
| `SELECT 1 = ANY (SELECT n, txt FROM set_t WHERE set_name='base')` | error 1241 / `21000`, `Operand should contain 1 column(s)` | one error row, code 1241 |
| `SELECT 1 = ANY (SELECT missing_col FROM set_t)` | error 1054 / `42S22`, unknown column | one error row, code 1054 |
| `SELECT 1 = ANY (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` | one error row, code 1235 |
| `SELECT 1 = ANY (SELECT n, txt FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` | one error row, code 1235 |
| `SELECT 1 = SOME (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` | one error row, code 1235 |
| `SELECT 1 = ALL (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` | one error row, code 1235 |
| `SELECT 1 <=> ANY (SELECT n FROM set_t WHERE set_name='base')` | syntax error 1064 / `42000` | one error row, code 1064 |

`LIMIT` diagnostics take precedence over scalar operand-width diagnostics for
quantified subquery forms.

Deferred shapes should not be mis-executed. Correlated quantified subqueries,
row operands, `TABLE`/`VALUES` subqueries, CTEs, set operations, and DML
contexts are valid or partially valid MySQL features outside this slice. MyLite
should return deterministic unsupported-feature diagnostics for those shapes
until their own specs and tests are ready.

### Metadata

Quantified subquery comparisons expose MySQL integer boolean metadata:

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
| `SELECT val > ANY (SELECT n FROM set_t WHERE set_name='base') AS any_result FROM outer_t LIMIT 0` | `any_result` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT val >= ALL (SELECT n FROM set_t WHERE set_name='base') AS all_result FROM outer_t LIMIT 0` | `all_result` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT val <> SOME (SELECT n FROM set_t WHERE set_name='base') AS some_result FROM outer_t LIMIT 0` | `some_result` | `LONGLONG` | `1` | `BINARY NUM` |

## Existing MyLite State

The parser and AST represent quantified comparisons. Parser tests cover `ANY`,
`SOME`, `ALL`, lowercase `any`, identifier fallback for `any` and `some` when
they are not followed by a parenthesized select subquery, and MySQL-compatible
rejection of null-safe quantified comparisons such as `<=> ANY`.

The first runtime slice executes uncorrelated scalar quantified comparisons in
no-table scalar `SELECT` and the current table-backed `SELECT` projection,
`WHERE`, join `ON`, `HAVING`, and `ORDER BY` expression contexts. The evaluator
handles the MySQL-supported operators `=`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`,
uses nullable boolean metadata, preserves comparison-warning order through
short-circuit evaluation, and shares the existing subquery diagnostics for
multi-column output and inner `LIMIT`.

The separate row quantified alias slice implements row `= ANY` / `= SOME` and
row `<>` / `!= ALL` by reusing row membership semantics. General row quantified
comparisons, correlated subqueries, DML contexts, and broader query surfaces
remain deferred.

## Parser and AST Design

No broad grammar expansion is required. The current parser already accepts the
core shape and builds a `MYLITE_SQL_AST_QUANTIFIED_COMPARISON` node with:

- child 0: scalar left operand
- child 1: inner `SELECT` statement
- `operator_kind`: comparison operator
- `subquery_quantifier`: `ANY`, `SOME`, or `ALL`

The intended Lemon-style shape is:

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

subquery(A) ::= LPAREN(L) select_statement(B) RPAREN(R). {
    A = mylite_sql_parser_make_subquery(state, L, B, R);
}
```

The snippet intentionally uses a quantified-specific operator nonterminal
without `NULL_SAFE_EQ`. If the implementation keeps the existing shared
`comparison_operator`, the analyzer must still reject `NULL_SAFE_EQ` before
execution with MySQL-compatible behavior.

The analyzer, not the parser, should enforce this slice's runtime limitations:
scalar left operands only, one-column subquery output, no outer references, no
inner `LIMIT`, and no unsupported inner query features.

## Runtime Design

### Analysis

Before execution, validation should:

1. Resolve the left operand in the outer expression context.
2. Reject row left operands for this slice.
3. Reject `NULL_SAFE_EQ` quantified operators as invalid MySQL syntax if the
   parser still accepts them.
4. Resolve the subquery as its own query block.
5. Reject any subquery outer reference for this slice.
6. Require exactly one visible subquery output column; otherwise raise 1241 /
   `21000`.
7. Reject `LIMIT` inside the quantified subquery with 1235 / `42000`.
8. Build nullable boolean result metadata with no origin fields.

### Evaluation

Evaluation for each outer row:

1. Evaluate the scalar left operand.
2. Execute the uncorrelated right subquery in its own query block.
3. For each right-side row, compare the left value with the right value using
   the existing MySQL expression comparison and conversion-warning machinery.
4. Treat a `NULL` left or right value as an unknown comparison result for all
   supported quantified operators.
5. For `ANY` and `SOME`, return `1` on the first true comparison.
6. For `ANY` and `SOME`, if no true comparison is found, return `NULL` when at
   least one comparison was unknown, otherwise return `0`.
7. For `ALL`, return `0` on the first false comparison.
8. For `ALL`, if no false comparison is found, return `NULL` when at least one
   comparison was unknown, otherwise return `1`.
9. Apply the empty-subquery identity before left-value nullability changes the
   outcome: empty `ANY`/`SOME` returns `0`, and empty `ALL` returns `1`.

The implementation may evaluate an uncorrelated subquery once per statement
when doing so preserves warnings and errors. It must not cache away observable
comparison warnings. A simple first implementation may stream the subquery for
each outer row; an optimized implementation can materialize right-side values
and replay comparisons in MySQL-compatible order.

### Warning Order

Warning behavior is part of the compatibility contract. For supported
conversion cases, warnings should appear in the order MySQL would encounter
candidate comparisons for the executed plan. A streaming implementation over
the current subquery row order is acceptable for this slice. If a later
optimizer changes subquery materialization order, it must keep warning counts
and ordering covered by compatibility tests.

### Storage and Performance

This feature needs no persistent storage or `.mylite` file-format change.

The implementation may need statement-owned temporary storage for:

- one-column subquery result values
- per-row comparison temporaries
- warning records generated while comparing left and right values
- boolean result descriptors for visible and hidden expressions

The first slice may use nested-loop evaluation. The design should leave room
for future semijoin, antijoin, and materialized-subquery planning, but those
optimizations must not leak into observable result, warning, error, or metadata
behavior.

## Test Plan

### Parser Tests

Parser coverage already asserts the core grammar. The implementation phase
should preserve those tests and add the MySQL-invalid null-safe quantified
boundary if the grammar is narrowed:

| SQL | Expected parser/analyzer outcome |
| --- | --- |
| `SELECT val > ANY (SELECT n FROM set_t);` | quantified comparison, `GREATER`, quantifier `ANY` |
| `SELECT val <> SOME (SELECT n FROM set_t);` | quantified comparison, `NOT_EQUAL`, quantifier `SOME` |
| `SELECT val >= ALL (SELECT n FROM set_t);` | quantified comparison, `GREATER_EQUAL`, quantifier `ALL` |
| `SELECT val > any /* comment */ (SELECT n FROM set_t);` | case-insensitive quantified comparison |
| `SELECT any, some FROM t;` | `any` and `some` remain identifiers outside quantified-subquery context |
| `SELECT val <=> ANY (SELECT n FROM set_t);` | MySQL-compatible syntax error or pre-execution rejection; do not execute |

### Runtime Result Tests

Runtime tests should use the fixture above and compare MyLite rows against
MySQL 8.4.9 for at least:

| SQL | Expected result |
| --- | --- |
| `SELECT 10 = ANY (SELECT n FROM set_t WHERE set_name='base')` | `1` |
| `SELECT 10 <> SOME (SELECT n FROM set_t WHERE set_name='base')` | `1` |
| `SELECT 20 >= ALL (SELECT n FROM set_t WHERE set_name='base' AND n IS NOT NULL)` | `1` |
| `SELECT 20 >= ALL (SELECT n FROM set_t WHERE set_name='base')` | `NULL` |
| `SELECT 7 > ANY (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT 7 > ALL (SELECT n FROM set_t WHERE set_name='missing')` | `1` |
| `SELECT NULL > ANY (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT NULL > ALL (SELECT n FROM set_t WHERE set_name='missing')` | `1` |
| `SELECT 7 > ANY (SELECT n FROM set_t WHERE set_name='order')` | `NULL` |
| `SELECT 7 > ALL (SELECT n FROM set_t WHERE set_name='order')` | `0` |
| `SELECT id, val > ANY (SELECT n FROM set_t WHERE set_name='base'), val >= ALL (SELECT n FROM set_t WHERE set_name='base') FROM outer_t ORDER BY id` | `(1,NULL,0)`, `(2,1,NULL)`, `(3,NULL,NULL)`, `(4,NULL,0)`, `(5,1,NULL)` |
| `SELECT id FROM outer_t WHERE val > ANY (SELECT n FROM set_t WHERE set_name='base') ORDER BY id` | `2`, `5` |
| `SELECT id FROM outer_t WHERE val >= ALL (SELECT n FROM set_t WHERE set_name='base' AND n IS NOT NULL) ORDER BY id` | `2`, `5` |
| `SELECT o.id, j.id FROM outer_t AS o JOIN join_t AS j ON j.outer_id=o.id AND j.marker = ANY (SELECT n FROM set_t WHERE set_name='base') ORDER BY o.id, j.id` | `(1,201)`, `(2,202)` |
| `SELECT grp, COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) = ANY (SELECT n FROM set_t WHERE set_name='having') ORDER BY grp` | `(1,2)` |
| `SELECT id FROM outer_t ORDER BY val <> SOME (SELECT n FROM set_t WHERE set_name='order') DESC, id` | `1`, `4`, `5`, `2`, `3` |
| `SELECT 10 = ANY (SELECT DISTINCT n FROM set_t WHERE set_name IN ('base','order'))` | `1` |
| `SELECT 2 = ALL (SELECT COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) >= 2)` | `1` |

### Runtime Warning Tests

Runtime warning tests should compare result rows, warning counts, warning
codes, and warning messages:

| SQL | Expected rows | Expected warnings |
| --- | --- | --- |
| `SELECT 0 > ANY (SELECT txt FROM set_t WHERE set_name='warn')` | `0` | two 1292 warnings: `'1x'`, `'abc'` |
| `SELECT 1 = ANY (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | one 1292 warning: `'1x'` |
| `SELECT 0 > ALL (SELECT txt FROM set_t WHERE set_name='warn')` | `0` | one 1292 warning: `'1x'` |
| `SELECT 2 > ALL (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | two 1292 warnings: `'1x'`, `'abc'` |
| `SELECT val > ANY (SELECT txt FROM set_t WHERE set_name='warn') FROM outer_t ORDER BY id` | `1`, `1`, `NULL`, `1`, `1` | four 1292 warnings, one for each non-`NULL` left value |

### Runtime Diagnostic Tests

Runtime diagnostic tests should compare error code, SQLSTATE, message, and
`SHOW WARNINGS` behavior:

| SQL | Expected behavior |
| --- | --- |
| `SELECT 1 = ANY (SELECT n, txt FROM set_t WHERE set_name='base')` | error 1241 / `21000`; `SHOW WARNINGS` has one error row |
| `SELECT 1 = ANY (SELECT missing_col FROM set_t)` | error 1054 / `42S22`; `SHOW WARNINGS` has one error row |
| `SELECT 1 = ANY (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`; `SHOW WARNINGS` has one error row |
| `SELECT 1 = SOME (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`; `SHOW WARNINGS` has one error row |
| `SELECT 1 = ALL (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`; `SHOW WARNINGS` has one error row |
| `SELECT 1 <=> ANY (SELECT n FROM set_t WHERE set_name='base')` | syntax error 1064 / `42000`; `SHOW WARNINGS` has one error row |
| `SELECT id FROM outer_t WHERE val > ANY (SELECT n FROM set_t WHERE n=outer_t.val)` | deferred correlated subquery diagnostic; do not execute as an uncorrelated subquery |
| `SELECT id FROM outer_t WHERE (val,grp) = ANY (SELECT n, n FROM set_t)` | handled by the row quantified alias slice; do not execute as scalar quantified comparison |

### Metadata Tests

Metadata tests should use the public result descriptor API and assert:

| SQL | Expected metadata |
| --- | --- |
| `SELECT val > ANY (SELECT n FROM set_t WHERE set_name='base') AS any_result FROM outer_t LIMIT 0` | label `any_result`; no origin fields; `LONGLONG`; length `1`; decimals `0`; charset id `63`; nullable; `BINARY` and `NUM` flags |
| `SELECT val >= ALL (SELECT n FROM set_t WHERE set_name='base') AS all_result FROM outer_t LIMIT 0` | same boolean metadata under label `all_result` |
| `SELECT val <> SOME (SELECT n FROM set_t WHERE set_name='base') AS some_result FROM outer_t LIMIT 0` | same boolean metadata under label `some_result` |

### Regression Boundaries

The runtime PR for this spec should also assert that existing Task 29 behavior
still passes:

- scalar subquery empty result is `NULL`
- scalar subquery multi-column error remains 1241
- scalar subquery multi-row error remains 1242
- `EXISTS` does not evaluate the select list for existence
- scalar `IN` / `NOT IN` subquery truth tables still pass
- hidden `ORDER BY` subquery expressions do not alter visible metadata
- quantified subqueries with `LIMIT` use error 1235, matching scalar `IN`
  subqueries

## Compatibility Status

The scalar quantified subquery comparison first slice is implemented.

Implemented coverage is uncorrelated scalar `ANY` / `SOME` / `ALL` comparisons
in no-table scalar `SELECT` and the current table-backed `SELECT` projection,
`WHERE`, join `ON`, `HAVING`, and `ORDER BY` expression contexts.

General row operands outside the accepted aliases, correlation, DML contexts,
derived tables, `TABLE`/`VALUES` subqueries, CTEs, set operations, null-safe
quantified comparison execution, and optimizer-specific behavior remain
separate features. The narrow MySQL-accepted row alias forms are specified in
`docs/specs/row-quantified-subquery-comparisons/specs.md`.
