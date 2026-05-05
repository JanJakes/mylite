# LEFT and RIGHT outer joins

## Scope

Task 27 extends Task 26's base-table join runtime with MySQL-compatible
`LEFT` and `RIGHT` outer joins for `SELECT`. The implementation should reuse
the Task 26 table-reference tree, name resolver, expression evaluator, result
metadata model, warning list, `WHERE`, `ORDER BY`, `LIMIT`, and no-group
aggregate paths while adding null-extension semantics.

In scope for the first implementation slice:

- `SELECT select_list FROM table_reference_list`
- user base tables created by MyLite's supported `CREATE TABLE` subset
- selected-schema and schema-qualified table resolution inherited from
  table-backed `SELECT`
- table aliases using `AS alias` or a bare identifier alias
- `LEFT JOIN`, `LEFT OUTER JOIN`, `RIGHT JOIN`, and `RIGHT OUTER JOIN`
- required `ON search_condition` or `USING (column_list)` join specifications
  for `LEFT` and `RIGHT` joins
- null extension for the non-preserved operand of an outer join
- `ON` predicate evaluation before null extension
- `WHERE` predicate evaluation after null extension
- `USING` equality semantics, duplicate `USING` names, and missing-column
  diagnostics
- `SELECT *`, qualified wildcards, `USING` coalesced columns, and result
  metadata for outer joined base-table columns
- unqualified, table-qualified, alias-qualified, and schema-table-qualified
  column references over outer joined row sources
- alias hiding and duplicate alias diagnostics
- MySQL's explicit-join precedence over comma joins, including `ON` operand
  visibility diagnostics
- `ORDER BY`, `LIMIT`, and `OFFSET` after joined row production and `WHERE`
  filtering
- no-group aggregate queries such as `COUNT(*)` over the produced outer-join
  row source, matching the Task 26 aggregate slice
- warning-list and warning-count behavior for supported scalar expressions
  evaluated inside `ON`, `WHERE`, and `ORDER BY`
- deterministic unsupported diagnostics for accepted but deferred
  table-reference surfaces

Out of scope for this slice:

- `NATURAL` joins, including `NATURAL LEFT JOIN` and `NATURAL RIGHT JOIN`
- `STRAIGHT_JOIN`
- ODBC `{ OJ ... }` escaped table references
- parenthesized table-reference groups
- derived tables, `LATERAL`, table functions, subqueries, CTEs, and views
- information-schema and performance-schema joins
- partitions and index hints
- optimizer hints and optimizer pushdown
- `DISTINCT`, set operations, windows, locking clauses, and `SELECT ... INTO`
- multi-table `UPDATE` and `DELETE`
- broad collation edge cases beyond the currently supported scalar expression
  and metadata surface

Task 27 is not syntax-only support. It is complete only when runtime tests
cover rows, metadata, warnings, errors, and side effects for the base-table
slice above.

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `JOIN` clause:
  https://dev.mysql.com/doc/refman/8.4/en/join.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, Problems with Column Aliases:
  https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html
- MySQL 8.4 C API Developer Guide, C API Basic Data Structures:
  https://dev.mysql.com/doc/c-api/8.4/en/c-api-data-structures.html
- MySQL 8.4 C API Developer Guide, `mysql_fetch_field()`:
  https://dev.mysql.com/doc/c-api/8.4/en/mysql-fetch-field.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/case-expression/specs.md`
  - `docs/specs/aggregate-grouping/specs.md`
  - `docs/specs/inner-joins/specs.md`

Observed behavior was verified on 2026-05-02 against MySQL 8.4.9 in Docker
container `mylite-mysql-849`, using:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot \
  --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot \
  --column-type-info -vvv --force
```

The verified server reported:

```text
VERSION(): 8.4.9
@@version_comment: MySQL Community Server - GPL
@@sql_mode: ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,
  NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Test fixture

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_task27_outer_joins;
CREATE DATABASE mylite_task27_outer_joins;
USE mylite_task27_outer_joins;

CREATE TABLE l (
  id INT PRIMARY KEY,
  shared INT,
  name VARCHAR(20) NOT NULL,
  only_l INT,
  nullable INT NULL
);

CREATE TABLE r (
  id INT PRIMARY KEY,
  l_id INT,
  shared INT,
  name VARCHAR(20) NOT NULL,
  only_r INT,
  flag INT NULL,
  score INT
);

CREATE TABLE e (
  id INT PRIMARY KEY,
  shared INT,
  note VARCHAR(20)
);

CREATE TABLE warn_l (id INT PRIMARY KEY);
CREATE TABLE warn_r (s VARCHAR(10));

INSERT INTO l VALUES
  (1, 10, 'alpha', 100, NULL),
  (2, 20, 'beta', 200, 5),
  (3, 30, 'gamma', 300, NULL);

INSERT INTO r VALUES
  (10, 1, 10, 'one', 1000, NULL, 7),
  (20, 2, 20, 'two', 2000, 5, 15),
  (21, 2, 20, 'two-b', 2100, NULL, 25),
  (40, 4, 40, 'orphan', 4000, 5, 35),
  (50, NULL, NULL, 'null-key', 5000, NULL, 45);

INSERT INTO e VALUES
  (1, 10, 'e-one'),
  (2, 20, 'e-two'),
  (5, 50, 'e-five');

INSERT INTO warn_l VALUES (1), (2);
INSERT INTO warn_r VALUES ('bad'), ('2x');
```

Result order without `ORDER BY` is not a semantic guarantee. Test cases must
include deterministic `ORDER BY` clauses when row order matters.

### Syntax forms

`LEFT` and `RIGHT` accept optional `OUTER` and require a join specification.

| SQL | MySQL result |
| --- | --- |
| `SELECT COUNT(*) FROM l LEFT JOIN r ON l.id = r.l_id` | `4` |
| `SELECT COUNT(*) FROM l LEFT OUTER JOIN r ON l.id = r.l_id` | `4` |
| `SELECT COUNT(*) FROM l RIGHT JOIN r ON l.id = r.l_id` | `5` |
| `SELECT COUNT(*) FROM l RIGHT OUTER JOIN r ON l.id = r.l_id` | `5` |
| `SELECT * FROM l LEFT JOIN r` | 1064 / `42000`, syntax error |

MyLite should parse `OUTER` as an optional marker that does not change the join
operator. Unlike inner joins, an outer join must have either `ON` or `USING`.

### Null extension and row preservation

For `LEFT JOIN`, all rows from the left operand are preserved. If no right-side
row satisfies the join condition, MySQL emits one joined row with every
right-side column set to `NULL`.

```sql
SELECT l.id AS l_id, r.id AS r_id, r.score
FROM l LEFT JOIN r ON l.id = r.l_id
ORDER BY l.id, r.id;
```

returns:

| l_id | r_id | score |
| --- | --- | --- |
| 1 | 10 | 7 |
| 2 | 20 | 15 |
| 2 | 21 | 25 |
| 3 | `NULL` | `NULL` |

For `RIGHT JOIN`, all rows from the right operand are preserved. If no
left-side row satisfies the join condition, MySQL emits one joined row with
every left-side column set to `NULL`.

```sql
SELECT l.id AS l_id, r.id AS r_id, r.score
FROM l RIGHT JOIN r ON l.id = r.l_id
ORDER BY r.id, l.id;
```

returns:

| l_id | r_id | score |
| --- | --- | --- |
| 1 | 10 | 7 |
| 2 | 20 | 15 |
| 2 | 21 | 25 |
| `NULL` | 40 | 35 |
| `NULL` | 50 | 45 |

`ON` predicates use normal three-valued SQL truth. A pair matches only when
the predicate evaluates true. False and `NULL` do not match; the preserved
side still appears once with the other side null-extended.

```sql
SELECT l.id AS l_id, r.id AS r_id
FROM l LEFT JOIN r ON l.nullable = r.flag
ORDER BY l.id, r.id;
```

returns `(1,NULL)`, `(2,20)`, `(2,40)`, and `(3,NULL)`.

### `ON` versus `WHERE`

`ON` is applied while forming the join. `WHERE` is applied after the joined row
has been produced, including after null extension.

```sql
SELECT l.id AS l_id, r.id AS r_id
FROM l LEFT JOIN r ON l.id = r.l_id AND r.score >= 10
ORDER BY l.id, r.id;
```

returns `(1,NULL)`, `(2,20)`, `(2,21)`, and `(3,NULL)`.

```sql
SELECT l.id AS l_id, r.id AS r_id
FROM l LEFT JOIN r ON l.id = r.l_id
WHERE r.score >= 10
ORDER BY l.id, r.id;
```

returns `(2,20)` and `(2,21)`. The `WHERE` predicate rejects rows where the
right side is null-extended because `r.score >= 10` evaluates unknown.

### `USING` semantics

`USING (column_list)` requires each named column to exist on both operands.
For row matching it behaves like equality comparisons between corresponding
columns combined with logical `AND`. `NULL = NULL` does not match.

MySQL accepts duplicate names in the `USING` list after validating the named
column. The probe:

```sql
SELECT COUNT(*) FROM l LEFT JOIN r USING (shared, shared);
```

returned `4`, matching `USING (shared)`.

For unqualified column references, a `USING` column is exposed as one
coalesced column. Qualified references remain valid and refer to the named base
table's physical column.

```sql
SELECT shared, l.shared AS l_shared, r.shared AS r_shared,
       l.id AS l_id, r.id AS r_id
FROM l LEFT JOIN r USING (shared)
ORDER BY shared IS NULL, shared, r.id;
```

returns:

| shared | l_shared | r_shared | l_id | r_id |
| --- | --- | --- | --- | --- |
| 10 | 10 | 10 | 1 | 10 |
| 20 | 20 | 20 | 2 | 20 |
| 20 | 20 | 20 | 2 | 21 |
| 30 | 30 | `NULL` | 3 | `NULL` |

The analogous right join:

```sql
SELECT shared, l.shared AS l_shared, r.shared AS r_shared,
       l.id AS l_id, r.id AS r_id
FROM l RIGHT JOIN r USING (shared)
ORDER BY shared IS NULL, shared, r.id;
```

returns:

| shared | l_shared | r_shared | l_id | r_id |
| --- | --- | --- | --- | --- |
| 10 | 10 | 10 | 1 | 10 |
| 20 | 20 | 20 | 2 | 20 |
| 20 | 20 | 20 | 2 | 21 |
| 40 | `NULL` | 40 | `NULL` | 40 |
| `NULL` | `NULL` | `NULL` | `NULL` | 50 |

`SELECT *` expansion for `USING` joins must follow the preserved side of the
outer join:

- `LEFT JOIN ... USING`: coalesced `USING` columns from the left operand, then
  left-operand non-`USING` columns, then right-operand non-`USING` columns.
- `RIGHT JOIN ... USING`: coalesced `USING` columns from the right operand,
  then right-operand non-`USING` columns, then left-operand non-`USING`
  columns.

Observed metadata for `SELECT * FROM l LEFT JOIN r USING (shared) LIMIT 0`:

```text
shared:l, id:l, name:l, only_l:l, nullable:l,
id:r, l_id:r, name:r, only_r:r, flag:r, score:r
```

Observed metadata for `SELECT * FROM l RIGHT JOIN r USING (shared) LIMIT 0`:

```text
shared:r, id:r, l_id:r, name:r, only_r:r, flag:r, score:r,
id:l, name:l, only_l:l, nullable:l
```

For multi-column `USING`, coalesced columns follow the preserved operand's
column order, not the textual order of the `USING` list. Both
`USING (shared, id)` and `USING (id, shared)` reported coalesced columns in
the order `id`, `shared` for the fixture because `id` precedes `shared` in
the preserved table.

Qualified wildcards do not coalesce `USING` columns. The probe:

```sql
SELECT l.*, r.* FROM l LEFT JOIN r USING (shared) ORDER BY shared LIMIT 0;
```

failed with 1052 / `23000`: `Column 'shared' in order clause is ambiguous`,
because the projection exposes duplicate `shared` labels.

### Name resolution, aliases, and operand scope

Task 27 inherits Task 26 name-resolution rules:

- table aliases become the visible qualifiers
- an alias hides the base table name for qualification
- duplicate visible qualifiers fail with 1066 / `42000`
- unqualified column references fail with 1052 / `23000` when more than one
  visible column matches
- unknown or hidden qualified references fail with 1054 / `42S22`
- projection aliases are not visible inside `ON` or `WHERE`
- `ORDER BY` alias resolution continues to follow Task 18 behavior

Representative probes:

| SQL | MySQL diagnostic |
| --- | --- |
| `SELECT id FROM l LEFT JOIN r ON l.id = r.l_id` | 1052 / `23000`, `Column 'id' in field list is ambiguous` |
| `SELECT a.id FROM l AS a LEFT JOIN r AS b ON l.id = b.l_id` | 1054 / `42S22`, `Unknown column 'l.id' in 'on clause'` |
| `SELECT * FROM l AS x LEFT JOIN r AS x ON x.id = x.l_id` | 1066 / `42000`, `Not unique table/alias: 'x'` |
| `SELECT * FROM l LEFT JOIN r USING (missing)` | 1054 / `42S22`, `Unknown column 'missing' in 'from clause'` |

An `ON` predicate can reference only the operands of the explicit join to
which it belongs. Tables that appear later in the `FROM` clause are not
visible. Explicit joins bind more tightly than comma joins, so comma-left
tables are not operands of a following explicit join.

Representative probes:

| SQL | MySQL diagnostic |
| --- | --- |
| `SELECT l.id, r.id, e.id FROM l, r LEFT JOIN e ON l.id = e.id` | 1054 / `42S22`, `Unknown column 'l.id' in 'on clause'` |
| `SELECT l.id, r.id FROM l LEFT JOIN r ON l.id = e.id LEFT JOIN e ON e.id = l.id` | 1054 / `42S22`, `Unknown column 'e.id' in 'on clause'` |

### Metadata

For `ON` joins, unqualified `SELECT *` emits columns from the syntactic left
operand followed by columns from the syntactic right operand. `RIGHT JOIN`
does not reorder `ON` wildcard output.

Outer joins affect nullability metadata. MySQL clears `NOT_NULL` on columns
from the null-extended side, even when the base table column is declared
`NOT NULL` or is a primary key. It keeps `NOT_NULL` on columns from the
preserved side.

Observed metadata summary:

| SQL | `l.id` flags | `r.id` flags |
| --- | --- | --- |
| `SELECT l.id AS lid, r.id AS rid FROM l LEFT JOIN r ON l.id = r.l_id LIMIT 0` | includes `NOT_NULL PRI_KEY` | includes `PRI_KEY`, not `NOT_NULL` |
| `SELECT l.id AS lid, r.id AS rid FROM l RIGHT JOIN r ON l.id = r.l_id LIMIT 0` | includes `PRI_KEY`, not `NOT_NULL` | includes `NOT_NULL PRI_KEY` |

For `USING` joins, the coalesced column's table and origin metadata come from
the preserved side. `SELECT shared FROM l RIGHT JOIN r USING (shared) LIMIT 0`
reported `Table='r'` and `Org_table='r'`.

Alias metadata follows existing Task 23 and Task 26 behavior: the client-visible
`Table` value is the table alias when present, while `Org_table` remains the
base table name.

### Warnings and side effects

Outer join predicates share the same warning lifecycle as other expression
evaluation contexts. MyLite must not push predicates into SQLite or reorder
evaluation unless rows, warnings, and errors remain MySQL-compatible for the
supported slice.

Observed warning probe:

```sql
SELECT COUNT(*) AS warning_count_result
FROM warn_l LEFT JOIN warn_r ON warn_r.s = 1;
SHOW COUNT(*) WARNINGS;
SHOW WARNINGS;
```

The result count was `2`. MySQL reported two warnings:

| Code | Message |
| --- | --- |
| 1292 | `Truncated incorrect DOUBLE value: 'bad'` |
| 1292 | `Truncated incorrect DOUBLE value: '2x'` |

The warning count was not multiplied by the two preserved left rows because
the predicate depends only on the right operand. The implementation should
extend Task 26's staged predicate cache so outer joins preserve MySQL-visible
warning counts for operand-local predicates.

Read-only outer join `SELECT` statements must not change affected rows,
session last insert id, transaction state, schema state, or table metadata.

### Deferred MySQL surfaces

MySQL supports several outer-join-adjacent surfaces that Task 27 intentionally
leaves for later work:

| Surface | MySQL behavior | Task 27 behavior |
| --- | --- | --- |
| `NATURAL LEFT/RIGHT JOIN` | Supported; equivalent to a `USING` join over all common columns and uses outer-join coalescing. | Deferred with deterministic unsupported parse/runtime diagnostic. |
| Parenthesized table references | Supported and can change comma/explicit join operands, including outer joins. | Deferred. |
| Derived tables and subqueries in `FROM` | Supported with aliases. | Deferred. |
| ODBC `{ OJ ... }` escaped references | Accepted for compatibility. | Deferred. |
| Index hints and partitions | Supported in table references. | Deferred. |
| Multi-table `UPDATE`/`DELETE` joins | Supported by MySQL. | Deferred to DML-specific tasks. |

## MyLite parser and AST design

Task 26 already introduced the table-reference tree needed for outer joins.
Task 27 should extend that design instead of adding a parallel row-source
representation.

Recommended join operator enum additions:

```c
enum mylite_sql_ast_join_type {
    MYLITE_SQL_AST_JOIN_NONE,
    MYLITE_SQL_AST_JOIN_INNER,
    MYLITE_SQL_AST_JOIN_CROSS,
    MYLITE_SQL_AST_JOIN_COMMA,
    MYLITE_SQL_AST_JOIN_LEFT,
    MYLITE_SQL_AST_JOIN_RIGHT,
};
```

The existing `MYLITE_SQL_AST_JOIN_CONDITION_ON` and
`MYLITE_SQL_AST_JOIN_CONDITION_USING` variants are sufficient. The analyzer
must reject `LEFT` or `RIGHT` joins that lack a condition; the grammar should
prefer requiring the condition syntactically so MyLite reports a normal syntax
error for `SELECT * FROM l LEFT JOIN r`.

The `JOIN_EXPRESSION` node should continue to store:

1. left table reference
2. right table reference
3. join type
4. join condition

No storage-format or catalog change is needed.

## Lemon-style grammar snippets

These snippets describe MyLite's intended Task 27 grammar shape. They are
independently authored and are not copied from MySQL grammar.

```lemon
%type inner_join_operator { struct mylite_sql_parser_join_operator }
%type outer_join_operator { struct mylite_sql_parser_join_operator }

joined_table_reference(A) ::= table_factor(B). {
    A = B;
}
joined_table_reference(A) ::= joined_table_reference(B) inner_join_operator(C)
        table_factor(D) opt_inner_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}
joined_table_reference(A) ::= joined_table_reference(B) outer_join_operator(C)
        table_factor(D) outer_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}

outer_join_operator(A) ::= LEFT(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_LEFT);
}
outer_join_operator(A) ::= LEFT(T) OUTER JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_LEFT);
}
outer_join_operator(A) ::= RIGHT(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_RIGHT);
}
outer_join_operator(A) ::= RIGHT(T) OUTER JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_RIGHT);
}

outer_join_condition(A) ::= ON(T) expression(B). {
    A = mylite_sql_parser_make_join_on_condition(state, T, B);
}
outer_join_condition(A) ::= USING(T) LPAREN using_column_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_join_using_condition(state, T, B, R);
}
```

The existing `table_reference_list` / `joined_table_reference` split must stay
intact so `LEFT` and `RIGHT` joins keep the same higher precedence over comma
joins as Task 26 explicit joins.

Deferred grammar should remain rejected or produce explicit unsupported
diagnostics:

```lemon
/* Deferred: parenthesized table-reference groups. */
table_factor ::= LPAREN table_references RPAREN.

/* Deferred: natural, straight, and ODBC escaped joins. */
joined_table_reference ::= joined_table_reference NATURAL LEFT JOIN table_factor.
joined_table_reference ::= joined_table_reference NATURAL RIGHT JOIN table_factor.
joined_table_reference ::= joined_table_reference STRAIGHT_JOIN table_factor opt_join_condition.
escaped_table_reference ::= LBRACE OJ table_reference RBRACE.

/* Deferred: derived tables, lateral tables, partitions, and index hints. */
table_factor ::= table_subquery table_alias.
table_factor ::= LATERAL table_subquery table_alias.
table_factor ::= table_name PARTITION LPAREN identifier_list RPAREN opt_table_alias.
table_factor ::= table_name opt_table_alias index_hint_list.
```

## Runtime design

### Binding and table scope

Preparing an outer-joined `SELECT` should use the Task 26 table-scope builder:

1. Resolve each base table against the selected schema or explicit schema.
2. Load table and column catalog rows in ordinal order.
3. Assign each base table a stable internal table slot.
4. Choose the visible qualifier: alias when present, otherwise base table
   name.
5. Reject duplicate visible qualifiers with a 1066-style not-unique-table-alias
   diagnostic.
6. Keep origin schema/table/column identity separate from visible alias
   metadata.

The resolver should retain MyLite's existing policy: schema/table/alias
qualifiers are byte-preserving and case-sensitive, while column-name lookup is
case-insensitive.

### Join execution

The Task 26 staged row-source engine should be extended to carry join nodes
rather than treating every explicit join as inner. For each outer join node:

- produce all matching row combinations exactly as an inner join would
- track whether each preserved-side row combination found at least one match
- after all candidate rows for the non-preserved side have been tested, emit
  one null-extended row when no match was found
- evaluate the node's `ON` or `USING` condition before null extension
- evaluate later joins against the joined row produced by the outer join,
  including null-extended columns

`RIGHT JOIN` may be implemented directly or internally normalized to a left
join only if observable output remains MySQL-compatible. For `ON` joins,
`SELECT *` output order must remain syntactic left operand columns followed by
syntactic right operand columns. For `USING` joins, wildcard output and
coalesced metadata must use the right preserved side.

Outer join matching must use existing MyLite expression semantics for
three-valued truth, conversion warnings, and error promotion. A condition
matches only when it evaluates true.

### `USING` output model

Task 26's `USING` representation needs one additional property: the preserved
side for the join. The output-column planner should derive a visible output
sequence for each table-reference subtree:

- inner/cross/comma joins: existing Task 26 order
- left `USING`: coalesced columns from the left subtree, left unique columns,
  right unique columns
- right `USING`: coalesced columns from the right subtree, right unique
  columns, left unique columns
- `ON` joins: syntactic left subtree columns, then syntactic right subtree
  columns

The coalesced value should be read from the preserved side for outer joins.
For matched rows, both sides compare equal for non-`NULL` values. For
unmatched rows, the non-preserved side is `NULL`.

### Metadata nullability

The result descriptor builder must account for null extension:

- columns from a preserved side keep their base nullability flags
- columns from a null-extended side must be nullable in result metadata, even
  when the base column is `NOT NULL` or a primary key
- coalesced `USING` columns use the preserved side's metadata and origin
- unique columns from the non-preserved side have `NOT_NULL` cleared
- aliases still affect visible table metadata but not origin metadata

This nullability adjustment applies recursively. If a subtree can be
null-extended by an outer join above it, all of that subtree's output columns
become nullable at that higher result boundary.

### Predicate placement

`ON` predicates are join-local and must be evaluated before null extension.
`WHERE` predicates must run after the full joined row is produced. This
distinction is user-visible for left/right joins and must not be optimized
away.

`ORDER BY` and `LIMIT` run after `WHERE`, as in Task 18 and Task 26.

### Aggregates

Task 26 supports aggregate calls over joined row sources. Task 27 keeps that
behavior for outer joins, including grouped aggregate queries after
null-extension. Group reference resolution and `ONLY_FULL_GROUP_BY` validation
are plan-wide.

### Warnings

Task 26 introduced staged predicate caching so warning counts are not
multiplied by unrelated comma-left or later join prefixes. Task 27 should
reuse and extend that mechanism. The cache key must include the row identity
of every table that can affect the predicate value and exclude tables outside
the predicate's operand range.

Null-extension itself must not create warnings. Warnings arise from evaluating
the `ON`, `WHERE`, or `ORDER BY` expressions.

## Test matrix

The implementation should add runtime tests against MySQL-verified expected
behavior for at least the following cases:

| Area | Test case | Expected behavior |
| --- | --- | --- |
| Syntax | `LEFT JOIN`, `LEFT OUTER JOIN`, `RIGHT JOIN`, `RIGHT OUTER JOIN` | Parse and execute with identical row counts for `OUTER` synonyms. |
| Syntax | `LEFT JOIN` or `RIGHT JOIN` without `ON`/`USING` | 1064 / `42000` syntax error. |
| LEFT rows | `l LEFT JOIN r ON l.id = r.l_id` | All left rows preserved; unmatched right side null-extended. |
| RIGHT rows | `l RIGHT JOIN r ON l.id = r.l_id` | All right rows preserved; unmatched left side null-extended. |
| Three-valued `ON` | `l.nullable = r.flag` | `NULL` predicate results do not match; preserved side still emits. |
| Predicate placement | Filter in `ON` vs the same filter in `WHERE` | `ON` keeps unmatched preserved rows; `WHERE` can remove null-extended rows. |
| `USING` left | `LEFT JOIN ... USING (shared)` | Coalesced `shared` from left for unmatched left rows. |
| `USING` right | `RIGHT JOIN ... USING (shared)` | Coalesced `shared` from right for unmatched right rows. |
| Duplicate `USING` | `USING (shared, shared)` | Accepted after validation; same rows as one `shared`. |
| Missing `USING` | `USING (missing)` | 1054 / `42S22`, unknown column in from clause. |
| Wildcard `ON` | `SELECT * FROM l RIGHT JOIN r ON ... LIMIT 0` | Syntactic left columns then syntactic right columns. |
| Wildcard `USING` left | `SELECT * FROM l LEFT JOIN r USING (shared) LIMIT 0` | Coalesced/left unique/right unique order. |
| Wildcard `USING` right | `SELECT * FROM l RIGHT JOIN r USING (shared) LIMIT 0` | Coalesced/right unique/left unique order. |
| Multi-column `USING` | `USING (shared, id)` and `USING (id, shared)` | Coalesced order follows preserved table column order. |
| Qualified wildcard | `SELECT l.*, r.* FROM ... USING(shared)` | No coalescing; duplicate labels remain. |
| Qualified wildcard ambiguity | `ORDER BY shared` after `SELECT l.*, r.* ... USING(shared)` | 1052 / `23000`, ambiguous order clause. |
| Metadata nullability | `l.id`, `r.id` under left/right `ON` joins | `NOT_NULL` cleared only on null-extended side. |
| Metadata origin | Coalesced `shared` under right `USING` | `Table` and `Org_table` from right/preserved side. |
| Alias hiding | Base table qualifier after alias in `ON` | 1054 / `42S22`, unknown column in on clause. |
| Duplicate alias | Same alias on both operands | 1066 / `42000`. |
| Ambiguous field | `SELECT id FROM l LEFT JOIN r ON ...` | 1052 / `23000`, ambiguous field list. |
| Operand scope | Later table referenced in earlier `ON` | 1054 / `42S22`, unknown column in on clause. |
| Comma precedence | `l, r LEFT JOIN e ON l.id = e.id` | 1054 / `42S22`, comma-left table outside operand scope. |
| Warning count | `warn_l LEFT JOIN warn_r ON warn_r.s = 1` | Result count `2`; two 1292 warnings. |
| ORDER/LIMIT | Outer join with deterministic order and limit | Ordering after `WHERE`; metadata preserved for `LIMIT 0`. |
| Aggregate | `COUNT(*)` over left/right join | Counts null-extended rows. |
| Grouping | `GROUP BY` or `HAVING` over outer join | Groups the joined row source after null-extension and applies normal aggregate semantics. |

## Implementation handoff notes

- Extend the parser/AST with `LEFT` and `RIGHT` join types; do not introduce a
  second outer-join node hierarchy.
- Keep the grammar condition mandatory for outer joins so missing conditions
  surface as syntax errors.
- Preserve Task 26 operand scoping for `ON`; this is especially important for
  comma precedence and later-table references.
- Treat `RIGHT JOIN` carefully: row preservation, `USING` wildcard order, and
  coalesced metadata are right-side based, but `ON` wildcard output remains in
  syntactic table order.
- Add a nullability overlay to result descriptors rather than mutating catalog
  column metadata.
- Reuse MyLite-owned staged scanning; SQLite should not decide predicate
  placement for this slice.
- Extend existing `USING` output planning to operate on table-reference
  subtrees, not just flat base-table ranges.
- Keep no-storage-impact behavior explicit: outer joins change only statement
  planning/execution and result descriptors.
- If the first implementation keeps the existing grouped-join limitation,
  tests must assert the documented unsupported diagnostic for grouped outer
  joins.

## Compatibility status

Task 27 is implemented for the base-table `SELECT` slice described above. The
implementation extends the Task 26 table-reference tree with `LEFT` and
`RIGHT` join types, requires `ON` or `USING` for outer joins in the grammar,
adds a MyLite-owned outer-join row-source path with null extension, preserves
`ON` before null extension and `WHERE` after it, applies preserved-side
`USING` coalescing, adjusts result metadata nullability for null-extended
sides, and includes runtime tests for rows, metadata, warnings, diagnostics,
ordering, limits, and aggregate counts.

Compatibility remains partial for the explicitly deferred surfaces above:
natural joins, parenthesized table references, derived tables, index hints, and
optimizer behavior.
