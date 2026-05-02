# UNION query expressions

## Scope

This feature starts MyLite query-expression support with the smallest useful
set-operation slice: top-level `SELECT` operands connected by `UNION`.

In scope:

- top-level `SELECT ... UNION SELECT ...` query expressions
- `UNION`, `UNION ALL`, and `UNION DISTINCT`
- one or more `UNION` operators in a left-associated chain
- operands limited to the `SELECT` surfaces already executable in MyLite
- operand-local duplicate modes such as `SELECT DISTINCT ...` where the
  operand itself is already supported
- parenthesized `SELECT` operands
- operand-local `ORDER BY` and `LIMIT` only when the operand is parenthesized
- global `ORDER BY`, `LIMIT row_count`, `LIMIT offset,row_count`, and
  `LIMIT row_count OFFSET offset` after the final operand
- duplicate elimination over the complete visible result row
- validation that all operands return the same number of columns
- output labels from the first operand
- union result descriptors with no base-table origin metadata
- MySQL-compatible diagnostics and warning preservation for the first slice

Out of scope:

- `INTERSECT` and `EXCEPT`
- non-recursive and recursive CTEs
- `TABLE` and `VALUES` operands
- locking clauses, including `FOR UPDATE`, `FOR SHARE`, and
  `LOCK IN SHARE MODE`
- `SELECT ... INTO` and query-expression `INTO`
- query expressions as derived tables or view definitions
- query expressions inside scalar, row, `IN`, `ANY`, `SOME`, `ALL`, or
  `EXISTS` subqueries
- correlated interactions introduced by set operations
- `INSERT ... SELECT`, `REPLACE ... SELECT`, and CTAS query expressions
- aggregate functions in global union `ORDER BY`
- optimizer behavior, temporary-table strategy, stable ordering without a
  complete global `ORDER BY`, and streaming/early-stop optimizations
- exhaustive type aggregation across all MySQL data types, character sets,
  collations, numeric precisions, temporal types, binary strings, JSON, spatial
  values, and generated expression metadata

The first implementation should not mark broad query-expression grammar as
supported. It should mark only the `UNION` subset above, and only after parser,
runtime, metadata, diagnostics, and MySQL-runtime comparison tests are present.

## Sources

- MySQL 8.4 Reference Manual, Set Operations with `UNION`, `INTERSECT`, and
  `EXCEPT`: https://dev.mysql.com/doc/refman/8.4/en/set-operations.html
- MySQL 8.4 Reference Manual, Parenthesized Query Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/parenthesized-query-expressions.html
- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/select-distinct/specs.md`
  - `docs/specs/aggregate-grouping/specs.md`
  - `docs/specs/inner-joins/specs.md`
  - `docs/specs/outer-joins/specs.md`
  - `docs/specs/subqueries/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force
```

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Existing MyLite context

The current parser represents a `SELECT` as `MYLITE_SQL_AST_SELECT_STATEMENT`.
That node already carries operand-local duplicate mode, projection list, table
references, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, and `LIMIT` children.
Subqueries currently wrap a parenthesized `select_statement`; there is no
query-expression AST layer yet.

The runtime SELECT path already owns row production, wildcard expansion,
projection descriptors, duplicate elimination for `SELECT DISTINCT`, ordering,
limiting, warnings, and supported subquery expression evaluation for the
current SELECT surfaces. The union implementation should reuse those query
block facilities and add a query-expression layer above them. It should not
teach lexer/parser code about SQLite execution details.

## MySQL 8.4.9 behavior summary

### Fixture

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_union_probe;
CREATE DATABASE mylite_union_probe
  CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE mylite_union_probe;

CREATE TABLE left_t (
  id INT PRIMARY KEY,
  n INT NULL,
  txt VARCHAR(10) COLLATE utf8mb4_0900_ai_ci NULL,
  bin_txt VARCHAR(10) COLLATE utf8mb4_bin NULL
);

CREATE TABLE right_t (
  id INT PRIMARY KEY,
  n INT NULL,
  label VARCHAR(10) COLLATE utf8mb4_0900_ai_ci NULL,
  bin_label VARCHAR(10) COLLATE utf8mb4_bin NULL
);

INSERT INTO left_t VALUES
  (1, 10, 'alpha', 'A'),
  (2, 10, 'Alpha', 'a'),
  (3, 20, 'beta', 'b'),
  (4, NULL, NULL, NULL),
  (5, 30, 'gamma', 'g');

INSERT INTO right_t VALUES
  (10, 10, 'alpha', 'A'),
  (11, 20, 'Beta', 'B'),
  (12, 40, 'delta', 'd'),
  (13, NULL, NULL, NULL);
```

Rows from a set operation are unordered unless a global `ORDER BY` makes the
order deterministic. Tests must include a complete global ordering whenever
row order matters.

### Duplicate modes

`UNION` without a modifier behaves as `UNION DISTINCT`. Duplicate elimination
uses the complete visible result row, with `NULL` values collapsing into one
duplicate key.

| SQL | Expected result |
| --- | --- |
| `SELECT n AS val FROM left_t WHERE id IN (1,3,4) UNION SELECT n FROM right_t WHERE id IN (10,11,13) ORDER BY val` | `NULL`, `10`, `20` |
| `SELECT n AS val FROM left_t WHERE id IN (1,2) UNION ALL SELECT n FROM right_t WHERE id=10 ORDER BY val` | `10`, `10`, `10` |
| `SELECT n AS val FROM left_t WHERE id IN (1,2) UNION DISTINCT SELECT n FROM right_t WHERE id=10 ORDER BY val` | `10` |

For a chain, each set operator consumes the accumulated left result and the
next operand. A `DISTINCT` operator removes duplicates from everything to its
left plus its right operand; a following `ALL` appends rows without duplicate
elimination.

| SQL | Expected result |
| --- | --- |
| `SELECT 1 AS v UNION ALL SELECT 1 UNION DISTINCT SELECT 1 ORDER BY v` | `1` |
| `SELECT 1 AS v UNION DISTINCT SELECT 1 UNION ALL SELECT 1 ORDER BY v` | `1`, `1` |

String duplicate identity follows the output expression collation. Under the
fixture, the case-insensitive column collapses `alpha` and `Alpha`; the binary
collated column keeps `A` and `a` separate.

| SQL | Expected result |
| --- | --- |
| `SELECT txt AS s FROM left_t WHERE id IN (1,2) UNION SELECT label FROM right_t WHERE id=10 ORDER BY s` | `alpha` |
| `SELECT bin_txt AS s FROM left_t WHERE id IN (1,2) UNION SELECT bin_label FROM right_t WHERE id=10 ORDER BY s` | `A`, `a` |

### Column count validation

Every operand must return the same number of columns. MySQL rejects a mismatch
before returning rows:

| SQL | Expected diagnostic |
| --- | --- |
| `SELECT 1 UNION SELECT 1, 2` | error 1222 / `21000`, different number of columns |

`SHOW COUNT(*) WARNINGS` reports one warning-list entry for this error.

### Output labels and metadata

Result column labels come from the first operand. Aliases in later operands do
not rename the union output.

Metadata probes with `--column-type-info -vvv` showed that union result fields
do not expose base-table origin metadata, even when both operands read base
columns. For:

```sql
SELECT n AS first_alias FROM left_t WHERE id=1
UNION ALL
SELECT n AS second_alias FROM right_t WHERE id=10
LIMIT 0;
```

the single result field is:

| Property | Value |
| --- | --- |
| label | `first_alias` |
| database/table/origin table | empty |
| type | `LONG` |
| collation | `binary (63)` |
| length | `11` |
| decimals | `0` |
| flags | `NUM` |

`UNION` and `UNION ALL` produce the same visible metadata for this probe.

With `SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci`, string metadata probes
showed:

| SQL | Metadata expectation |
| --- | --- |
| `SELECT txt AS s FROM left_t WHERE id=1 UNION ALL SELECT label FROM right_t WHERE id=10 LIMIT 0` | label `s`; empty origin; `VAR_STRING`; collation id `255`; length `40` |
| `SELECT bin_txt AS s FROM left_t WHERE id=1 UNION ALL SELECT bin_label FROM right_t WHERE id=10 LIMIT 0` | label `s`; empty origin; `VAR_STRING`; collation id `255`; length `40`; `BINARY` flag |
| `SELECT CAST('a' AS CHAR(1)) AS c UNION ALL SELECT CAST('bbbb' AS CHAR(4)) LIMIT 0` | label `c`; empty origin; `VAR_STRING`; collation id `255`; length `16` |
| `SELECT 1 AS mixed UNION ALL SELECT 'abc' LIMIT 0` | label `mixed`; empty origin; `VAR_STRING`; collation id `255`; length `12`; `NOT_NULL` flag |

The first implementation should cover identical numeric descriptors, identical
string descriptors, simple string widening, and the verified numeric/string
literal coercion above. Broader descriptor aggregation is explicitly deferred
until separate MySQL-runtime coverage exists.

### Global ORDER BY, LIMIT, and OFFSET

A global `ORDER BY` after the final operand sorts the union result. Global
`LIMIT` then applies to the sorted result.

| SQL | Expected result |
| --- | --- |
| `SELECT id AS sort_id, n AS val FROM left_t WHERE id IN (1,3,5) UNION ALL SELECT id, n FROM right_t WHERE id IN (10,11,12) ORDER BY sort_id DESC LIMIT 3 OFFSET 1` | `(11,20)`, `(10,10)`, `(5,30)` |
| `SELECT id AS sort_id FROM left_t WHERE id IN (1,3,5) UNION ALL SELECT id FROM right_t WHERE id IN (10,11,12) ORDER BY 1 LIMIT 2, 3` | `5`, `10`, `11` |

Global `ORDER BY` can use output labels from the union result, expressions over
those labels, and one-based ordinals.

| SQL | Expected result |
| --- | --- |
| `SELECT n AS a FROM left_t UNION ALL SELECT n FROM right_t ORDER BY a LIMIT 3` | `NULL`, `NULL`, `10` |
| `SELECT n AS val FROM left_t UNION ALL SELECT n FROM right_t ORDER BY val IS NULL, val LIMIT 5` | `10`, `10`, `10`, `20`, `20` |
| `SELECT n AS val FROM left_t UNION ALL SELECT n FROM right_t ORDER BY val + 0 LIMIT 5` | `NULL`, `NULL`, `10`, `10`, `10` |
| `SELECT n AS val FROM left_t UNION ALL SELECT n FROM right_t ORDER BY 1 DESC LIMIT 3` | `40`, `30`, `20` |

Global `ORDER BY` cannot use table-qualified names from any operand. If the
first operand aliases a column, the global order must use the output alias
rather than the original column name.

| SQL | Expected diagnostic |
| --- | --- |
| `SELECT n AS a FROM left_t UNION ALL SELECT n FROM right_t ORDER BY n LIMIT 3` | error 1054 / `42S22`, unknown column `n` in order clause |
| `SELECT n AS a FROM left_t UNION ALL SELECT n FROM right_t ORDER BY left_t.n LIMIT 3` | error 1250 / `42000`, operand table used in global order clause |
| `SELECT n AS x, id AS x FROM left_t UNION ALL SELECT n, id FROM right_t ORDER BY x LIMIT 1` | error 1052 / `23000`, ambiguous `x` |
| `SELECT n AS val FROM left_t UNION ALL SELECT n FROM right_t ORDER BY 2 LIMIT 1` | error 1054 / `42S22`, unknown column `2` in order clause |

### Operand-local ORDER BY and LIMIT

An operand with its own `ORDER BY` or `LIMIT` must be parenthesized. The local
clauses run before the set operation and do not define final output order.
Final order still requires a global `ORDER BY`.

| SQL | Expected result |
| --- | --- |
| `(SELECT id AS sort_id FROM left_t ORDER BY id DESC LIMIT 2) UNION ALL (SELECT id FROM right_t ORDER BY id ASC LIMIT 1) ORDER BY sort_id` | `4`, `5`, `10` |
| `(SELECT 1 AS v) UNION (SELECT 2) ORDER BY v` | `1`, `2` |
| `(SELECT 1 AS v LIMIT 1) UNION SELECT 2 ORDER BY v LIMIT 1` | `1` |
| `SELECT 1 AS v UNION (SELECT 2 LIMIT 1) ORDER BY v DESC LIMIT 1` | `2` |
| `((SELECT 1 AS v)) UNION ((SELECT 2)) ORDER BY v` | `1`, `2` |

MySQL rejects an unparenthesized operand-local `ORDER BY` before a `UNION`:

| SQL | Expected diagnostic |
| --- | --- |
| `SELECT id FROM left_t ORDER BY id LIMIT 1 UNION ALL SELECT id FROM right_t` | error 1064 / `42000`, syntax error near the union |

### Warnings

A successful union without warning-producing expressions leaves the warning
count at zero:

| SQL | Expected warning count |
| --- | --- |
| `SELECT 1 AS v UNION SELECT 1` | `0` |

Warnings produced while evaluating operands are preserved:

| SQL | Expected result | Expected warnings |
| --- | --- | --- |
| `SELECT 1/0 AS v UNION ALL SELECT 2` | `NULL`, `2.0000` | one warning, code 1365, division by zero |

Syntax errors and validation errors populate the warning list with the error
row, matching existing MyLite diagnostic policy for SQL errors.

### Syntax edge cases

The first slice should match these MySQL diagnostics:

| SQL | Expected diagnostic |
| --- | --- |
| `SELECT 1 UNION ALL DISTINCT SELECT 1` | error 1064 / `42000` |
| `SELECT 1 UNION SELECT 2 ORDER BY 1 OFFSET 1` | error 1064 / `42000` |

`INTERSECT`, `EXCEPT`, `TABLE`, `VALUES`, CTEs, locking clauses, and `INTO`
should remain rejected or explicitly unsupported until their own specs exist.

## Lemon-style grammar snippets

These snippets describe the intended MyLite grammar shape for this feature.
They are independently authored for MyLite and are not copied from MySQL
grammar.

The key parser change is to introduce a query-expression layer above a SELECT
query block. Global `ORDER BY` and `LIMIT` belong to the query expression.
Operand-local `ORDER BY` and `LIMIT` are accepted only for parenthesized
operands.

```lemon
%type query_expression { struct mylite_sql_ast_node * }
%type query_expression_body { struct mylite_sql_ast_node * }
%type query_primary { struct mylite_sql_ast_node * }
%type union_operator { struct mylite_sql_parser_union_operator }

statement(A) ::= query_expression(B). {
    A = B;
}

query_expression(A) ::= query_expression_body(B)
                        opt_order_by_clause(O)
                        opt_limit_clause(L). {
    A = mylite_sql_parser_make_query_expression(state, B, O, L);
}

query_expression_body(A) ::= query_primary(B). {
    A = B;
}

query_expression_body(A) ::= query_expression_body(L)
                             union_operator(U)
                             query_primary(R). {
    A = mylite_sql_parser_make_union_expression(state, L, U, R);
}

union_operator(A) ::= UNION(T). {
    A = mylite_sql_parser_make_union_operator(
        T, MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT);
}

union_operator(A) ::= UNION(T) ALL(M). {
    A = mylite_sql_parser_make_union_operator(
        T, M, MYLITE_SQL_AST_SET_DUPLICATES_ALL);
}

union_operator(A) ::= UNION(T) DISTINCT(M). {
    A = mylite_sql_parser_make_union_operator(
        T, M, MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT);
}

query_primary(A) ::= select_query_block(B). {
    A = B;
}

query_primary(A) ::= parenthesized_query_primary(B). {
    A = B;
}

parenthesized_query_primary(A) ::= LPAREN(L) select_query_block(B)
                                   opt_order_by_clause(O)
                                   opt_limit_clause(N)
                                   RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_query_primary(
        state, L, B, O, N, R);
}

parenthesized_query_primary(A) ::= LPAREN(L)
                                   parenthesized_query_primary(B)
                                   RPAREN(R). {
    A = mylite_sql_parser_wrap_parenthesized_query_primary(state, L, B, R);
}

select_query_block(A) ::= SELECT select_duplicate_mode(M)
                          select_item_list(L)
                          opt_from_clause(F)
                          opt_where_clause(W)
                          opt_group_by_clause(G)
                          opt_having_clause(H). {
    A = mylite_sql_parser_make_select_query_block(state, M, L, F, W, G, H);
}
```

The existing `select_statement` production can be retained as a compatibility
wrapper during refactoring, but it should no longer be the only statement-level
read grammar. `SELECT` duplicate modes and `UNION` duplicate modes must remain
separate AST concepts: `SELECT DISTINCT` removes duplicates inside one operand,
while `UNION DISTINCT` removes duplicates across set-operation inputs.

Deferred grammar should remain rejected or explicitly unsupported:

```lemon
/* Deferred set operators. */
query_expression_body ::= query_expression_body INTERSECT query_primary.
query_expression_body ::= query_expression_body EXCEPT query_primary.

/* Deferred query block kinds. */
query_primary ::= TABLE qualified_identifier.
query_primary ::= VALUES row_constructor_list.

/* Deferred CTEs and INTO. */
query_expression ::= WITH common_table_expression_list query_expression_body.
query_expression ::= query_expression_body INTO into_target.

/* Deferred derived table query expressions. */
table_factor ::= LPAREN query_expression RPAREN table_alias.

/* Deferred parenthesized set-operation expressions. */
query_primary ::= LPAREN query_expression_body RPAREN.

/* Deferred locking clauses. */
select_query_block ::= SELECT select_item_list FROM table_references locking_clause.
```

## Parser and AST design

Add query-expression AST nodes rather than overloading
`MYLITE_SQL_AST_SELECT_STATEMENT`:

- `MYLITE_SQL_AST_QUERY_EXPRESSION`: owns the query-expression body plus global
  `ORDER BY` and `LIMIT` children.
- `MYLITE_SQL_AST_UNION_EXPRESSION`: left child, right child, and a union
  duplicate mode.
- `MYLITE_SQL_AST_QUERY_PRIMARY`: optional wrapper when a parenthesized operand
  carries local `ORDER BY` or `LIMIT`.
- `MYLITE_SQL_AST_SET_DUPLICATES_ALL` and
  `MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT`: a new enum for set-operation
  duplicate mode.

The AST should preserve source spans for:

- the `UNION` token
- optional `ALL` or `DISTINCT`
- operand parentheses
- global and operand-local `ORDER BY` and `LIMIT`

These spans are needed for deterministic parser diagnostics and later error
messages. They also keep future `INTERSECT` and `EXCEPT` additions localized.

## Analyzer and runtime design

### Analysis

Analysis should proceed in this order:

1. Analyze each operand with the existing SELECT analyzer, including
   operand-local projection expansion, aliases, `WHERE`, grouping, `HAVING`,
   operand-local duplicate mode, and operand-local `ORDER BY`/`LIMIT`.
2. Reject any operand that uses a SELECT surface not already implemented.
3. Validate that every operand has the same visible column count. Mismatches
   use error 1222 / SQLSTATE `21000`.
4. Build union output descriptors from corresponding operand descriptors:
   - labels come from operand 1
   - schema, table, origin table, and origin column metadata are empty
   - identical numeric descriptors remain numeric
   - identical string descriptors remain string descriptors with the resolved
     collation and binary flag behavior covered by tests
   - the simple widening/coercion cases in this spec are supported
   - broader aggregation is deferred until MySQL-runtime coverage is added
5. Bind global `ORDER BY` against the union output, not against operand tables.
6. Reject table-qualified global `ORDER BY` references with error 1250.
7. Resolve unqualified global `ORDER BY` names against output labels. Duplicate
   labels are ambiguous with error 1052.
8. Resolve global one-based ordinals. Out-of-range ordinals are unknown-column
   errors, matching existing Task 18 behavior.
9. Reject aggregate functions in the global union `ORDER BY` with MySQL error
   3028 once aggregate expressions are accepted in that position.

Global `ORDER BY` expression binding may reuse the existing order-expression
binder, but the name-resolution environment must expose only union output
columns. Operand table names and aliases are not visible globally.

### Runtime

Runtime should evaluate operands through existing SELECT row producers and then
combine rows in query-expression order:

1. Start with rows from the first operand.
2. For `UNION ALL`, append rows from the right operand unchanged.
3. For `UNION` or `UNION DISTINCT`, combine the accumulated left rows with
   right operand rows and replace the accumulator with a distinct set.
4. Duplicate keys compare complete visible output tuples using MyLite values,
   `NULL` duplicate behavior, and the resolved output collation for each
   column.
5. Apply global `ORDER BY`.
6. Apply global `LIMIT`/`OFFSET`.
7. Expose the union output descriptors and affected rows `-1`.

The first implementation should favor correctness over pushdown. A
statement-owned materialized accumulator with MyLite value comparators is
acceptable for this slice. SQLite `UNION` pushdown should wait until metadata,
warnings, collation behavior, and duplicate identity have compatibility tests
proving equivalence.

Operand-local `LIMIT` must restrict that operand before the union operation.
Operand-local `ORDER BY` does not define final order. If it appears without a
local `LIMIT`, MyLite may skip the physical sort when doing so matches MySQL's
visible rows and warnings for the supported expression subset.

Warnings from operand execution, duplicate key evaluation, global order
expressions, and type conversion must be preserved in statement order as far as
MySQL behavior is verified. A successful union that produces no expression
warnings leaves the warning count at zero.

Storage impact is limited to statement-owned temporary memory. This feature
does not change the `.mylite` file format, schema catalog, or SQLite offset
model.

## Diagnostics

| Condition | MySQL behavior | MyLite requirement |
| --- | --- | --- |
| different operand column counts | error 1222 / `21000` | same code and warning-list entry |
| global `ORDER BY` uses original column name when first operand used an alias | error 1054 / `42S22` | same code and message context |
| global `ORDER BY` uses `table.column` from an operand | error 1250 / `42000` | same code and warning-list entry |
| duplicate output labels referenced by global `ORDER BY` | error 1052 / `23000` | same code and warning-list entry |
| out-of-range global `ORDER BY` ordinal | error 1054 / `42S22` | same code and warning-list entry |
| operand-local `ORDER BY`/`LIMIT` before `UNION` without parentheses | error 1064 / `42000` | syntax error |
| `UNION ALL DISTINCT` or other double set modifier | error 1064 / `42000` | syntax error |
| global `OFFSET` without `LIMIT` | error 1064 / `42000` | syntax error |
| successful union with no expression warnings | warning count `0` | same |
| operand expression warning, such as division by zero | result rows plus warning 1365 | same result and warning |

Tests should assert numeric error code and SQLSTATE first. Exact message text
should be asserted once the MyLite diagnostic layer exposes that fidelity for
the relevant error.

## Test plan

Add parser and runtime tests before marking this feature supported.

Parser tests:

- `SELECT 1 UNION SELECT 2`
- `SELECT 1 UNION ALL SELECT 2`
- `SELECT 1 UNION DISTINCT SELECT 2`
- `SELECT 1 UNION ALL SELECT 1 UNION DISTINCT SELECT 1`
- `(SELECT 1) UNION (SELECT 2) ORDER BY 1`
- `(SELECT id FROM t ORDER BY id LIMIT 1) UNION SELECT id FROM u`
- `SELECT id FROM t UNION (SELECT id FROM u LIMIT 1) ORDER BY 1 LIMIT 1`
- nested harmless parentheses such as `((SELECT 1)) UNION ((SELECT 2))`
- syntax error for `SELECT id FROM t ORDER BY id LIMIT 1 UNION SELECT id FROM u`
- syntax error for `SELECT 1 UNION ALL DISTINCT SELECT 1`
- syntax error or explicit unsupported diagnostic for `INTERSECT`, `EXCEPT`,
  `TABLE`, `VALUES`, CTEs, locking clauses, `INTO`, and derived-table query
  expressions

Runtime row tests using the fixture above:

| Case | SQL | Expected result |
| --- | --- | --- |
| default distinct | `SELECT n AS val FROM left_t WHERE id IN (1,3,4) UNION SELECT n FROM right_t WHERE id IN (10,11,13) ORDER BY val` | `NULL`, `10`, `20` |
| explicit all | `SELECT n AS val FROM left_t WHERE id IN (1,2) UNION ALL SELECT n FROM right_t WHERE id=10 ORDER BY val` | `10`, `10`, `10` |
| explicit distinct | `SELECT n AS val FROM left_t WHERE id IN (1,2) UNION DISTINCT SELECT n FROM right_t WHERE id=10 ORDER BY val` | `10` |
| mixed modes, distinct last | `SELECT 1 AS v UNION ALL SELECT 1 UNION DISTINCT SELECT 1 ORDER BY v` | `1` |
| mixed modes, all last | `SELECT 1 AS v UNION DISTINCT SELECT 1 UNION ALL SELECT 1 ORDER BY v` | `1`, `1` |
| case-insensitive collation | `SELECT txt AS s FROM left_t WHERE id IN (1,2) UNION SELECT label FROM right_t WHERE id=10 ORDER BY s` | `alpha` |
| binary collation | `SELECT bin_txt AS s FROM left_t WHERE id IN (1,2) UNION SELECT bin_label FROM right_t WHERE id=10 ORDER BY s` | `A`, `a` |
| global order and offset | `SELECT id AS sort_id, n AS val FROM left_t WHERE id IN (1,3,5) UNION ALL SELECT id, n FROM right_t WHERE id IN (10,11,12) ORDER BY sort_id DESC LIMIT 3 OFFSET 1` | `(11,20)`, `(10,10)`, `(5,30)` |
| comma limit offset | `SELECT id AS sort_id FROM left_t WHERE id IN (1,3,5) UNION ALL SELECT id FROM right_t WHERE id IN (10,11,12) ORDER BY 1 LIMIT 2, 3` | `5`, `10`, `11` |
| operand-local limit | `(SELECT id AS sort_id FROM left_t ORDER BY id DESC LIMIT 2) UNION ALL (SELECT id FROM right_t ORDER BY id ASC LIMIT 1) ORDER BY sort_id` | `4`, `5`, `10` |
| warning propagation | `SELECT 1/0 AS v UNION ALL SELECT 2` | `NULL`, `2.0000`; one 1365 warning |

Runtime diagnostic tests:

| Case | SQL | Expected diagnostic |
| --- | --- | --- |
| column count mismatch | `SELECT 1 UNION SELECT 1, 2` | 1222 / `21000` |
| aliased column ordered by original name | `SELECT n AS a FROM left_t UNION ALL SELECT n FROM right_t ORDER BY n LIMIT 3` | 1054 / `42S22` |
| table-qualified global order | `SELECT n AS a FROM left_t UNION ALL SELECT n FROM right_t ORDER BY left_t.n LIMIT 3` | 1250 / `42000` |
| duplicate global order label | `SELECT n AS x, id AS x FROM left_t UNION ALL SELECT n, id FROM right_t ORDER BY x LIMIT 1` | 1052 / `23000` |
| bad ordinal | `SELECT n AS val FROM left_t UNION ALL SELECT n FROM right_t ORDER BY 2 LIMIT 1` | 1054 / `42S22` |
| unparenthesized operand order | `SELECT id FROM left_t ORDER BY id LIMIT 1 UNION ALL SELECT id FROM right_t` | 1064 / `42000` |
| bad set modifier sequence | `SELECT 1 UNION ALL DISTINCT SELECT 1` | 1064 / `42000` |
| offset without limit | `SELECT 1 UNION SELECT 2 ORDER BY 1 OFFSET 1` | 1064 / `42000` |

Metadata tests:

- `UNION ALL` and `UNION DISTINCT` over `n AS first_alias` expose label
  `first_alias`, no origin metadata, `LONG`, binary collation id `63`, length
  `11`, decimals `0`, and `NUM`.
- String union with `SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci` exposes no
  origin metadata, `VAR_STRING`, collation id `255`, byte length `40` for
  `VARCHAR(10)`, and preserves the observed binary flag for binary-collated
  inputs.
- `CAST('a' AS CHAR(1)) UNION ALL SELECT CAST('bbbb' AS CHAR(4)) LIMIT 0`
  exposes `VAR_STRING` length `16` under utf8mb4.
- `SELECT 1 AS mixed UNION ALL SELECT 'abc' LIMIT 0` exposes `VAR_STRING`
  length `12` and `NOT_NULL`.

Side-effect tests:

- The statement is read-only and reports affected rows `-1`.
- Session last insert id remains unchanged.
- `LIMIT 0` returns no rows while preserving result metadata.
- Warnings are cleared and owned using the same lifecycle as existing SELECT
  execution.

## Implementation handoff

1. Refactor parser read statements around `query_expression` while preserving
   existing plain SELECT behavior.
2. Add AST nodes and enum values for query expressions, union expressions, set
   duplicate modes, and parenthesized query primaries.
3. Keep SELECT duplicate mode and UNION duplicate mode separate.
4. Reuse existing SELECT analysis/execution for each operand.
5. Add union descriptor aggregation with first-operand labels and empty origin
   metadata.
6. Add column-count validation before row execution.
7. Implement materialized union accumulation with MyLite value comparators.
8. Bind global `ORDER BY` against union output descriptors only.
9. Apply global `LIMIT` after global ordering.
10. Add parser and runtime tests from this spec.
11. Update `COMPATIBILITY.md` from ❌ to 🟡 only after implementation and
    MySQL-runtime comparison tests land.

## Open risks and deferrals

- MySQL's full type aggregation is broad. This first slice records simple
  verified metadata expectations but defers exhaustive descriptor merging.
- Collation metadata in the mysql client exposes both collation id and flags;
  MyLite should assert the verified fields before broadening charset coverage.
- Operand-local `ORDER BY` without `LIMIT` is not a final ordering contract.
  Additional warning probes are needed before supporting exotic local order
  expressions beyond the current expression subset.
- Future `INTERSECT` precedence will require a query-term layer above union
  parsing. The first `UNION` implementation should avoid hard-coding a shape
  that makes that precedence difficult.
- Query expressions inside subqueries and derived tables need separate binding
  rules and should not be enabled accidentally by parser refactoring.
- Streaming optimizations and SQLite pushdown should wait until compatibility
  tests prove they preserve rows, metadata, warnings, and diagnostics.
