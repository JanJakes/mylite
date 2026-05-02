# Subquery `IN` Predicates

## Scope

This specification starts the next Task 29 subquery slice: executable scalar
`IN` and scalar `NOT IN` predicates whose right side is an uncorrelated table
subquery.

The first executable slice should support:

- scalar left operands from the currently supported expression subset,
  including constants, column references, aggregate outputs where already
  legal, and deterministic scalar expressions
- parenthesized `SELECT` subqueries that return one visible column and zero or
  more rows
- uncorrelated subqueries only; the subquery must not reference an outer query
  block
- no-table scalar `SELECT`
- table-backed `SELECT` projection expressions
- table-backed `SELECT WHERE`
- explicit join `ON`
- grouped `HAVING`
- hidden and visible `ORDER BY` expressions
- inner subquery row sources and clauses already supported by MyLite's current
  `SELECT` implementation, including simple table scans, joins, grouping,
  `HAVING`, `DISTINCT`, and `ORDER BY`

The first executable slice should not support:

- row left operands such as `(a,b) IN (SELECT ...)` or
  `ROW(a,b) IN (SELECT ...)`
- correlated subquery references
- `INSERT`, `UPDATE`, `DELETE`, `SET`, `DO`, stored-program, generated-column,
  check-constraint, default-expression, view, or trigger contexts
- quantified comparisons with `ANY`, `SOME`, or `ALL`
- derived tables, `LATERAL`, `TABLE`, or `VALUES` subqueries
- CTEs and set operations inside subqueries
- optimizer semijoin, antijoin, decorrelation, or index-driven execution

MySQL rejects `LIMIT` in subqueries consumed by `IN`, `NOT IN`, `ANY`, `SOME`,
and `ALL`. This slice should implement that validation as error 1235. If the
implementation deliberately cuts that diagnostic from the first runtime PR, it
must reject `IN` subqueries containing `LIMIT` as unsupported before execution
and keep the MySQL 1235 behavior as the next required diagnostic.

## Sources

- MySQL 8.4 Reference Manual, Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subqueries.html
- MySQL 8.4 Reference Manual, Subqueries with `ANY`, `IN`, or `SOME`:
  https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html
- MySQL 8.4 Reference Manual, Subquery Errors:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html
- MySQL 8.4 Reference Manual, The Subquery as Scalar Operand:
  https://dev.mysql.com/doc/refman/8.4/en/scalar-subqueries.html
- MySQL 8.4 Reference Manual, Optimizing Subqueries:
  https://dev.mysql.com/doc/refman/8.4/en/subquery-optimization.html
- Existing MyLite umbrella spec:
  `docs/specs/subqueries/specs.md`

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
DROP DATABASE IF EXISTS mylite_task29_subquery_in;
CREATE DATABASE mylite_task29_subquery_in
  CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE mylite_task29_subquery_in;

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
  ('order',20,'20'),
  ('order',NULL,NULL),
  ('having',2,'2'),
  ('having',3,'3'),
  ('text',NULL,'alpha'),
  ('text',NULL,'beta'),
  ('warn',NULL,'1x'),
  ('warn',NULL,'abc');

INSERT INTO join_t VALUES
  (201,1,10),
  (202,2,20),
  (203,3,30),
  (204,4,NULL),
  (205,5,5);
```

### Truth Semantics

For subqueries, `expr IN (subquery)` is equivalent to `expr = ANY
(subquery)`. `expr NOT IN (subquery)` is equivalent to `expr <> ALL
(subquery)`, not `expr <> ANY (subquery)`.

Scalar `IN` and `NOT IN` return MySQL boolean integers, with `NULL` when the
membership test is unknown. The important cases are:

- a matching non-`NULL` right-side value makes `IN` true and `NOT IN` false
- an empty right-side result makes `IN` false and `NOT IN` true, even when the
  left operand is `NULL`
- a `NULL` left operand with a non-empty right side makes both predicates
  `NULL`
- no match plus at least one unknown comparison makes both predicates `NULL`
- no match and no unknown comparisons makes `IN` false and `NOT IN` true

Verified results:

| SQL | Expected result |
| --- | --- |
| `SELECT 10 IN (SELECT n FROM set_t WHERE set_name='base')` | `1` |
| `SELECT 5 IN (SELECT n FROM set_t WHERE set_name='base')` | `NULL` |
| `SELECT 5 NOT IN (SELECT n FROM set_t WHERE set_name='base')` | `NULL` |
| `SELECT 5 IN (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT 5 NOT IN (SELECT n FROM set_t WHERE set_name='missing')` | `1` |
| `SELECT NULL IN (SELECT n FROM set_t WHERE set_name='base')` | `NULL` |
| `SELECT NULL IN (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT NULL NOT IN (SELECT n FROM set_t WHERE set_name='missing')` | `1` |

### Supported SELECT Contexts

The same predicate semantics apply wherever this slice exposes scalar
subquery `IN` predicates.

Verified no-table scalar `SELECT`:

| SQL | Expected result |
| --- | --- |
| `SELECT 10 IN (SELECT n FROM set_t WHERE set_name='base') AS match_in, 5 NOT IN (SELECT n FROM set_t WHERE set_name='missing') AS miss_not_in_empty` | `(1,1)` |

Verified table-backed projection:

| SQL | Expected result |
| --- | --- |
| `SELECT id, val IN (SELECT n FROM set_t WHERE set_name='base') AS in_base, val NOT IN (SELECT n FROM set_t WHERE set_name='base') AS not_in_base FROM outer_t ORDER BY id` | `(1,1,0)`, `(2,1,0)`, `(3,NULL,NULL)`, `(4,NULL,NULL)`, `(5,NULL,NULL)` |

Verified `WHERE` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t WHERE val IN (SELECT n FROM set_t WHERE set_name='base') ORDER BY id` | `1`, `2` |
| `SELECT id FROM outer_t WHERE val NOT IN (SELECT n FROM set_t WHERE set_name='missing') ORDER BY id` | `1`, `2`, `3`, `4`, `5` |

Verified join `ON` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT o.id, j.id FROM outer_t AS o JOIN join_t AS j ON j.outer_id=o.id AND j.marker IN (SELECT n FROM set_t WHERE set_name='base') ORDER BY o.id, j.id` | `(1,201)`, `(2,202)` |

Verified `HAVING` filtering:

| SQL | Expected result |
| --- | --- |
| `SELECT grp, COUNT(*) AS c FROM outer_t GROUP BY grp HAVING COUNT(*) IN (SELECT n FROM set_t WHERE set_name='having') ORDER BY grp` | `(1,2)` |

Verified hidden `ORDER BY` expression:

| SQL | Expected result |
| --- | --- |
| `SELECT id FROM outer_t ORDER BY val IN (SELECT n FROM set_t WHERE set_name='order') DESC, id` | `2`, `1`, `3`, `4`, `5` |

### Inner Subquery Clauses

`ORDER BY`, `DISTINCT`, grouping, and `HAVING` inside an `IN` subquery are
valid when the subquery has no `LIMIT` and returns one column.

Verified results:

| SQL | Expected result | Warnings |
| --- | --- | --- |
| `SELECT 10 IN (SELECT n FROM set_t WHERE set_name='base' ORDER BY n DESC)` | `1` | `0` |
| `SELECT 10 IN (SELECT DISTINCT n FROM set_t WHERE set_name IN ('base','order'))` | `1` | `0` |
| `SELECT 2 IN (SELECT COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) >= 2)` | `1` | `0` |

### Type Conversion and Warnings

Comparison warnings belong to the outer statement. MySQL compares candidates
until the predicate result is determined. This matters when conversion warning
counts are observable.

Verified warning behavior:

| SQL | Result | Warning count | Warning rows |
| --- | --- | --- | --- |
| `SELECT 2 IN (SELECT txt FROM set_t WHERE set_name='warn')` | `0` | `2` | two warnings, code 1292, for `'1x'` and `'abc'` |
| `SELECT 1 IN (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | `1` | one warning, code 1292, for `'1x'` |
| `SELECT 2 NOT IN (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | `2` | two warnings, code 1292, for `'1x'` and `'abc'` |
| `SELECT val IN (SELECT txt FROM set_t WHERE set_name='warn') FROM outer_t ORDER BY id` | `0`, `0`, `NULL`, `0`, `0` | `8` | two warnings for each non-`NULL` outer `val`; no warnings for the `NULL` left operand |

The first runtime implementation should preserve these warning counts for
supported expression types. If the comparison engine cannot yet produce a
specific conversion warning, the test should stay as an expected gap rather
than silently accepting a warning count mismatch.

### Diagnostics

An `IN` subquery used with a scalar left operand must return exactly one
column. It may return zero, one, or many rows. Multi-row results are normal for
`IN` and `NOT IN`; they are not scalar-subquery error 1242 cases.

Verified diagnostics:

| SQL | Expected behavior | `SHOW WARNINGS` |
| --- | --- | --- |
| `SELECT 1 IN (SELECT n, txt FROM set_t WHERE set_name='base')` | error 1241 / `21000`, `Operand should contain 1 column(s)` | one error row, code 1241 |
| `SELECT 1 IN (SELECT missing_col FROM set_t)` | error 1054 / `42S22`, unknown column | one error row, code 1054 |
| `SELECT 1 IN (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`, unsupported `LIMIT & IN/ALL/ANY/SOME subquery` | one error row, code 1235 |

Deferred shapes should not be mis-executed. In particular, correlated `IN`
subqueries and row left operands are valid MySQL features, but they are out of
scope for this slice and should receive MyLite's existing unsupported-feature
diagnostic until their own specs and tests are ready.

### Metadata

Scalar `IN` and `NOT IN` subquery predicates expose MySQL integer boolean
metadata:

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
| `SELECT val IN (SELECT n FROM set_t WHERE set_name='base') AS in_result FROM outer_t LIMIT 0` | `in_result` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT val NOT IN (SELECT n FROM set_t WHERE set_name='base') AS not_in_result FROM outer_t LIMIT 0` | `not_in_result` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT 5 IN (SELECT n FROM set_t WHERE set_name='missing') AS no_table_in LIMIT 0` | `no_table_in` | `LONGLONG` | `1` | `BINARY NUM` |
| `SELECT 2 IN (SELECT txt FROM set_t WHERE set_name='warn') AS numeric_text_in LIMIT 0` | `numeric_text_in` | `LONGLONG` | `1` | `BINARY NUM` |

## Parser and AST Design

No grammar expansion is required for this scalar `IN` / `NOT IN` slice. The
current parser already accepts `comparison_expression IN subquery` and
`comparison_expression NOT IN subquery`, and it represents those expressions as
binary expressions whose right child is the inner `SELECT` statement.

The relevant Lemon-style shape is:

```lemon
comparison_expression(A) ::= comparison_expression(B) IN(T) subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IN, C);
}

comparison_expression(A) ::= comparison_expression(B) NOT(T) IN subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_IN, C);
}

subquery(A) ::= LPAREN(L) select_statement(B) RPAREN(R). {
    A = mylite_sql_parser_make_subquery(state, L, B, R);
}
```

Implementation should distinguish expression-list `IN` from subquery `IN` by
the right child kind:

- expression-list `IN`: right child is an expression list
- subquery `IN`: right child is a `SELECT` statement

The analyzer, not the parser, should enforce this slice's runtime limitations:
scalar left operands only, one-column subquery output, no outer references, and
no unsupported inner query features.

## Runtime Design

### Analysis

Before execution, validation should:

1. Resolve the left operand in the outer expression context.
2. Resolve the subquery as its own query block.
3. Reject any subquery outer reference for this slice.
4. Reject row left operands as unsupported for this slice.
5. Require exactly one visible subquery output column; otherwise raise 1241 /
   `21000`.
6. Reject `LIMIT` inside the `IN` or `NOT IN` subquery with 1235 / `42000`,
   unless the first runtime PR explicitly defers that diagnostic and rejects
   the shape as unsupported before execution.
7. Build boolean result metadata with no origin fields.

### Evaluation

The expression evaluator should not treat subquery `IN` as expression-list
`IN`. A scalar subquery callback is also insufficient because `IN` accepts any
number of rows. Add or reuse a table-subquery evaluation path that can stream
or materialize one-column rows while preserving MySQL-visible behavior.

Evaluation for each outer row:

1. Evaluate the scalar left operand.
2. If the left operand is `NULL`, return `NULL` for a non-empty right side and
   return `0`/`1` for `IN`/`NOT IN` when the right side is empty.
3. Execute the uncorrelated right subquery in its own query block.
4. Compare the left value with each right value using the existing MySQL
   expression comparison and conversion-warning machinery.
5. For `IN`, return `1` on the first true equality comparison.
6. For `NOT IN`, return `0` on the first true equality comparison.
7. Track whether any comparison was unknown.
8. If no equality matched and any comparison was unknown, return `NULL`.
9. If no equality matched and no comparison was unknown, return `0` for `IN`
   and `1` for `NOT IN`.

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

Parser coverage should assert that already-supported grammar produces the
expected AST shape:

| SQL | Expected parser outcome |
| --- | --- |
| `SELECT 10 IN (SELECT n FROM set_t);` | binary `IN` expression with right child `SELECT` |
| `SELECT 10 NOT IN (SELECT n FROM set_t);` | binary `NOT IN` expression with right child `SELECT` |
| `SELECT id FROM outer_t WHERE val IN (SELECT n FROM set_t);` | subquery `IN` under `WHERE` |
| `SELECT id FROM outer_t JOIN join_t ON marker IN (SELECT n FROM set_t);` | subquery `IN` under join `ON` |
| `SELECT grp, COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) IN (SELECT n FROM set_t);` | subquery `IN` under `HAVING` |
| `SELECT id FROM outer_t ORDER BY val NOT IN (SELECT n FROM set_t);` | hidden subquery `NOT IN` order key |

Parser coverage should keep deferred shapes visible:

| SQL | First-slice expectation |
| --- | --- |
| `SELECT (val,grp) IN (SELECT n, grp FROM set_t);` | may parse, but runtime/analyzer rejects row left operands as unsupported |
| `SELECT val > ANY (SELECT n FROM set_t);` | may parse, but quantified execution remains unsupported |
| `SELECT val IN (SELECT n FROM set_t WHERE set_t.n=outer_t.val);` | may parse, but correlated binding remains unsupported |

### Runtime Result Tests

Runtime tests should use the fixture above and compare MyLite rows against
MySQL 8.4.9 for at least:

| SQL | Expected result |
| --- | --- |
| `SELECT 10 IN (SELECT n FROM set_t WHERE set_name='base')` | `1` |
| `SELECT 5 IN (SELECT n FROM set_t WHERE set_name='base')` | `NULL` |
| `SELECT 5 NOT IN (SELECT n FROM set_t WHERE set_name='base')` | `NULL` |
| `SELECT 5 IN (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT 5 NOT IN (SELECT n FROM set_t WHERE set_name='missing')` | `1` |
| `SELECT NULL IN (SELECT n FROM set_t WHERE set_name='missing')` | `0` |
| `SELECT id, val IN (SELECT n FROM set_t WHERE set_name='base') FROM outer_t ORDER BY id` | `(1,1)`, `(2,1)`, `(3,NULL)`, `(4,NULL)`, `(5,NULL)` |
| `SELECT id FROM outer_t WHERE val IN (SELECT n FROM set_t WHERE set_name='base') ORDER BY id` | `1`, `2` |
| `SELECT id FROM outer_t WHERE val NOT IN (SELECT n FROM set_t WHERE set_name='missing') ORDER BY id` | `1`, `2`, `3`, `4`, `5` |
| `SELECT o.id, j.id FROM outer_t AS o JOIN join_t AS j ON j.outer_id=o.id AND j.marker IN (SELECT n FROM set_t WHERE set_name='base') ORDER BY o.id, j.id` | `(1,201)`, `(2,202)` |
| `SELECT grp, COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) IN (SELECT n FROM set_t WHERE set_name='having') ORDER BY grp` | `(1,2)` |
| `SELECT id FROM outer_t ORDER BY val IN (SELECT n FROM set_t WHERE set_name='order') DESC, id` | `2`, `1`, `3`, `4`, `5` |
| `SELECT 'alpha' IN (SELECT txt FROM set_t WHERE set_name='text'), 'zeta' NOT IN (SELECT txt FROM set_t WHERE set_name='text')` | `(1,1)` |
| `SELECT 10 IN (SELECT n FROM set_t WHERE set_name='base' ORDER BY n DESC)` | `1` |
| `SELECT 10 IN (SELECT DISTINCT n FROM set_t WHERE set_name IN ('base','order'))` | `1` |
| `SELECT 2 IN (SELECT COUNT(*) FROM outer_t GROUP BY grp HAVING COUNT(*) >= 2)` | `1` |

### Runtime Warning Tests

Runtime warning tests should compare result rows, warning counts, warning
codes, and warning messages:

| SQL | Expected rows | Expected warnings |
| --- | --- | --- |
| `SELECT 2 IN (SELECT txt FROM set_t WHERE set_name='warn')` | `0` | two 1292 warnings: `'1x'`, `'abc'` |
| `SELECT 1 IN (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | one 1292 warning: `'1x'` |
| `SELECT 2 NOT IN (SELECT txt FROM set_t WHERE set_name='warn')` | `1` | two 1292 warnings: `'1x'`, `'abc'` |
| `SELECT val IN (SELECT txt FROM set_t WHERE set_name='warn') FROM outer_t ORDER BY id` | `0`, `0`, `NULL`, `0`, `0` | eight 1292 warnings, two for each non-`NULL` left value |

### Runtime Diagnostic Tests

Runtime diagnostic tests should compare error code, SQLSTATE, message, and
`SHOW WARNINGS` behavior:

| SQL | Expected behavior |
| --- | --- |
| `SELECT 1 IN (SELECT n, txt FROM set_t WHERE set_name='base')` | error 1241 / `21000`; `SHOW WARNINGS` has one error row |
| `SELECT 1 IN (SELECT missing_col FROM set_t)` | error 1054 / `42S22`; `SHOW WARNINGS` has one error row |
| `SELECT 1 IN (SELECT n FROM set_t WHERE set_name='base' LIMIT 1)` | error 1235 / `42000`; `SHOW WARNINGS` has one error row, unless explicitly deferred as described in Scope |
| `SELECT id FROM outer_t WHERE val IN (SELECT n FROM set_t WHERE n=outer_t.val)` | deferred correlated subquery diagnostic; do not execute as an uncorrelated subquery |
| `SELECT id FROM outer_t WHERE (val,grp) IN (SELECT n, n FROM set_t)` | deferred row-left-operand diagnostic; do not execute as scalar `IN` |

### Metadata Tests

Metadata tests should use the public result descriptor API and assert:

| SQL | Expected metadata |
| --- | --- |
| `SELECT val IN (SELECT n FROM set_t WHERE set_name='base') AS in_result FROM outer_t LIMIT 0` | label `in_result`; no origin fields; `LONGLONG`; length `1`; decimals `0`; charset id `63`; nullable; `BINARY` and `NUM` flags |
| `SELECT val NOT IN (SELECT n FROM set_t WHERE set_name='base') AS not_in_result FROM outer_t LIMIT 0` | same boolean metadata under label `not_in_result` |
| `SELECT 5 IN (SELECT n FROM set_t WHERE set_name='missing') AS no_table_in LIMIT 0` | same boolean metadata under label `no_table_in` |
| `SELECT 2 IN (SELECT txt FROM set_t WHERE set_name='warn') AS numeric_text_in LIMIT 0` | same boolean metadata under label `numeric_text_in` |

### Regression Boundaries

The runtime PR for this spec should also assert that existing Task 29 scalar
subquery and `EXISTS` behavior still passes:

- scalar subquery empty result is `NULL`
- scalar subquery multi-column error remains 1241
- scalar subquery multi-row error remains 1242
- `EXISTS` does not evaluate the select list for existence
- hidden `ORDER BY` subquery expressions do not alter visible metadata

## Compatibility Status

This is a start-feature specification. MyLite should continue to mark scalar
and row `IN` subquery execution as not implemented until the runtime slice,
MySQL-runtime comparison tests, and compatibility matrix update land.

Once implemented, the supported claim should be limited to uncorrelated scalar
`IN` / `NOT IN` subqueries in no-table scalar `SELECT` and the current
table-backed `SELECT` projection, `WHERE`, join `ON`, `HAVING`, and `ORDER BY`
expression contexts. Row operands, correlation, DML contexts, quantified
comparisons, derived tables, CTEs, set operations, and optimizer-specific
behavior remain separate features.
