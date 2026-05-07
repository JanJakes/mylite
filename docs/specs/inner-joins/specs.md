# Inner joins and comma joins

## Scope

Task 26 adds the first multi-table row source for `SELECT`. It extends the
current one-table table-backed `SELECT` runtime with MySQL-compatible inner
join semantics while keeping the implementation slice small enough to build on
the existing resolver, expression evaluator, result metadata, `WHERE`,
`ORDER BY`, and `LIMIT` machinery.

In scope for the first implementation slice:

- `SELECT select_list FROM table_reference_list`
- user base tables created by MyLite's supported `CREATE TABLE` subset
- selected-schema and schema-qualified table resolution inherited from the
  table-backed `SELECT` core
- table aliases using `AS alias` or a bare identifier alias
- `JOIN`, `INNER JOIN`, and `CROSS JOIN` between table references
- comma-separated table references
- optional `ON` predicates for explicit inner and cross joins
- optional `USING (column_list)` predicates for explicit inner and cross joins
- explicit joins without `ON` or `USING`, producing a Cartesian product
- comma joins as Cartesian products, with row filtering supplied by `WHERE`
- MySQL's precedence rule where explicit joins bind more tightly than comma
  joins
- unqualified, table-qualified, alias-qualified, and schema-table-qualified
  column references over the joined row source
- ambiguous-column diagnostics in the select list, `ON`, `WHERE`, and
  `ORDER BY`
- `SELECT *`, qualified wildcards, duplicate output labels, `USING` coalesced
  columns, and result metadata for joined base-table columns
- `WHERE` after join predicate evaluation, using the currently supported
  scalar expression subset
- `ORDER BY`, `LIMIT`, and `OFFSET` after joined row production and `WHERE`
  filtering, using the currently supported expression and bound subset
- warning-list and warning-count behavior for supported scalar expressions
  evaluated inside `ON`, `WHERE`, and `ORDER BY`
- deterministic unsupported diagnostics for syntactically accepted but
  intentionally deferred join-table surfaces

Out of scope for the first implementation slice:

- outer joins, including `LEFT`, `RIGHT`, and null-extension behavior
- `NATURAL` joins
- `STRAIGHT_JOIN`
- broad parenthesized table-reference groups over comma lists; parenthesized
  nested join operands are covered for the base-table join surface
- derived tables, `LATERAL`, table functions, subqueries, CTEs, and views
- information-schema and performance-schema joins
- partitions; index hints are parsed and ignored by the separate placeholder
  slice
- optimizer hints and optimizer pushdown
- join-order optimization beyond preserving MySQL-visible semantics
- `DISTINCT`, set operations, windows, locking clauses, and `SELECT ... INTO`
- multi-table `UPDATE` and `DELETE`
- broad collation edge cases beyond the currently supported scalar expression
  and metadata surface

Task 26 support is tracked as the executable base-table slice only after
runtime tests cover rows, metadata, warnings, errors, and statement side
effects. Parser acceptance alone is not join support.

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
  - `docs/specs/create-table-base-execution/specs.md`

Observed behavior was verified on 2026-05-02 against MySQL 8.4.9 in Docker
container `mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Test fixture

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_task26_inner_joins;
CREATE DATABASE mylite_task26_inner_joins;
USE mylite_task26_inner_joins;

CREATE TABLE l (
  id INT PRIMARY KEY,
  name VARCHAR(20),
  shared INT,
  only_l INT,
  nullable INT NULL
);

CREATE TABLE r (
  id INT PRIMARY KEY,
  l_id INT,
  name VARCHAR(20),
  shared INT,
  only_r INT,
  nullable INT NULL,
  score INT
);

CREATE TABLE p (
  id INT PRIMARY KEY,
  l_id INT,
  marker VARCHAR(10)
);

CREATE TABLE warn_l (id INT PRIMARY KEY);
CREATE TABLE warn_r (s VARCHAR(10));

INSERT INTO l VALUES
  (1, 'alpha', 10, 100, NULL),
  (2, 'beta', 20, 200, 5),
  (3, 'gamma', 30, 300, 5);

INSERT INTO r VALUES
  (10, 1, 'one', 10, 1000, NULL, 7),
  (20, 2, 'two', 20, 2000, 5, 15),
  (21, 2, 'two-b', 20, 2100, NULL, 25),
  (40, 4, 'missing', 40, 4000, 5, 35);

INSERT INTO p VALUES
  (100, 1, 'p-one'),
  (200, 2, 'p-two');

INSERT INTO warn_l VALUES (1);
INSERT INTO warn_r VALUES ('bad'), ('2x');
```

Result order without `ORDER BY` is not a semantic guarantee. Examples below
include explicit ordering when row order matters.

### Join forms and result rows

`JOIN`, `INNER JOIN`, and `CROSS JOIN` are inner join forms. Without `ON` or
`USING`, explicit joins and comma joins produce Cartesian products.

Representative runtime results:

| SQL | MySQL result |
| --- | --- |
| `SELECT l.id, r.id FROM l INNER JOIN r ON l.id = r.l_id ORDER BY l.id, r.id` | `(1,10)`, `(2,20)`, `(2,21)` |
| `SELECT COUNT(*) FROM l JOIN r` | `12` |
| `SELECT COUNT(*) FROM l CROSS JOIN r` | `12` |
| `SELECT l.id, r.id FROM l, r WHERE l.id = r.l_id ORDER BY l.id, r.id` | `(1,10)`, `(2,20)`, `(2,21)` |
| `SELECT l.id, r.id FROM l CROSS JOIN r ON l.id = r.l_id ORDER BY l.id, r.id` | `(1,10)`, `(2,20)`, `(2,21)` |

For inner joins, `ON` predicates keep a row pair only when the predicate
evaluates true. False and `NULL` predicate results reject the pair. The query:

```sql
SELECT l.id, r.id
FROM l JOIN r ON l.nullable = r.nullable
ORDER BY l.id, r.id;
```

returns `(2,20)`, `(2,40)`, `(3,20)`, and `(3,40)`. The `(1,10)` pair is not
returned even though both values are `NULL`, because `NULL = NULL` is unknown.

`WHERE`, `ORDER BY`, and `LIMIT` run after joined row production. The query:

```sql
SELECT l.id AS l_id, r.id AS r_id, r.score
FROM l JOIN r ON l.id = r.l_id
WHERE r.score >= 10
ORDER BY r.score DESC, r.id
LIMIT 2;
```

returns `(2,21,25)` and `(2,20,15)`.

### `USING` semantics

`USING (column_list)` requires each named column to exist on both operands of
the explicit join. For an inner join, it filters row pairs as if each named
column pair were compared with equality and combined with logical `AND`.

For `SELECT *`, `USING` also changes output shape. MySQL emits one coalesced
column for each common join column, then the noncoalesced columns from the left
operand, then the noncoalesced columns from the right operand. The coalesced
columns are ordered according to their order in the left operand's output, not
according to the text order in the `USING` list.

Observed `SELECT * FROM l JOIN r USING (shared) ORDER BY shared, r.id` output:

| shared | l.id | l.name | l.only_l | l.nullable | r.id | r.l_id | r.name | r.only_r | r.nullable | r.score |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 10 | 1 | alpha | 100 | `NULL` | 10 | 1 | one | 1000 | `NULL` | 7 |
| 20 | 2 | beta | 200 | 5 | 20 | 2 | two | 2000 | 5 | 15 |
| 20 | 2 | beta | 200 | 5 | 21 | 2 | two-b | 2100 | `NULL` | 25 |

The visible labels for that query are:

```text
shared, id, name, only_l, nullable, id, l_id, name, only_r, nullable, score
```

`SELECT * FROM l JOIN r USING (id, shared) LIMIT 0` and
`SELECT * FROM l JOIN r USING (shared, id) LIMIT 0` both report coalesced
columns in the order `id`, `shared`, because `id` precedes `shared` in `l`.

MySQL accepts duplicate names in the `USING` list. The probe
`SELECT * FROM l JOIN r USING (shared, shared)` produced the same rows and
output shape as `USING (shared)`. MyLite should treat duplicate `USING` names
as one join column after validating that the column exists on both sides.

Qualified references to `USING` columns remain valid. This query:

```sql
SELECT shared, l.shared AS l_shared, r.shared AS r_shared,
       l.id AS l_id, r.id AS r_id
FROM l JOIN r USING (shared)
ORDER BY shared, r.id;
```

returns `(10,10,10,1,10)`, `(20,20,20,2,20)`, and
`(20,20,20,2,21)`. The unqualified `shared` expression resolves to the
coalesced column; qualified expressions resolve to the named base table's
column. For an inner join, the coalesced value equals both qualified values
for matching non-`NULL` rows.

Qualified wildcards do not use `USING` coalescing. `SELECT l.*, r.* FROM
l JOIN r USING (shared)` exposes all columns from `l` followed by all columns
from `r`, including both `shared` columns. Consequently,
`ORDER BY shared` in that query fails with:

- 1052 / SQLSTATE `23000`: `Column 'shared' in order clause is ambiguous`

### Name resolution and aliases

A table alias becomes the visible table name for qualification and metadata.
When an alias is present, the base table name is hidden in joined queries just
as it is hidden in one-table `SELECT`.

Metadata for:

```sql
SELECT lefty.id AS left_id, righty.id AS right_id
FROM l AS lefty JOIN r AS righty ON lefty.id = righty.l_id
LIMIT 0;
```

reports `Table='lefty', Org_table='l'` for `left_id` and
`Table='righty', Org_table='r'` for `right_id`.

Unqualified column references search the joined table scope. If exactly one
visible table has a matching column, the reference resolves. If more than one
visible table has the column, MySQL reports an ambiguity error. If no visible
table has the column, MySQL reports an unknown-column error.

Representative diagnostics:

| SQL | MySQL diagnostic |
| --- | --- |
| `SELECT id FROM l JOIN r ON l.id = r.l_id` | 1052 / `23000`, `Column 'id' in field list is ambiguous` |
| `SELECT l.id, r.id FROM l JOIN r ON l.id = r.l_id WHERE id = 2` | 1052 / `23000`, `Column 'id' in where clause is ambiguous` |
| `SELECT l.id FROM l AS lefty JOIN r AS righty ON lefty.id = righty.l_id` | 1054 / `42S22`, `Unknown column 'l.id' in 'field list'` |
| `SELECT lefty.id FROM l AS lefty JOIN r AS righty ON l.id = righty.l_id` | 1054 / `42S22`, `Unknown column 'l.id' in 'on clause'` |
| `SELECT * FROM l AS x JOIN r AS x ON x.id = x.l_id` | 1066 / `42000`, `Not unique table/alias: 'x'` |

Column-name matching remains case-insensitive, following existing MyLite table
DDL and single-table `SELECT` behavior. On the verified Linux MySQL runtime,
table, schema, and alias qualifiers remain case-sensitive.

Projection aliases are not visible inside `ON` or `WHERE`. Existing Task 18
`ORDER BY` alias resolution should continue to apply after join support is
added, but ambiguity must be checked against duplicate projection labels and
joined base columns.

### `ON` operand visibility and comma precedence

An `ON` predicate can reference only the operands of the explicit join to
which it belongs. It cannot see tables that appear later in the `FROM` clause.
The query:

```sql
SELECT * FROM l JOIN r ON l.id = p.l_id JOIN p;
```

fails with:

- 1054 / SQLSTATE `42S22`: `Unknown column 'p.l_id' in 'on clause'`

Explicit joins bind more tightly than comma joins. MySQL interprets
`l, r JOIN p ON ...` as `l, (r JOIN p ON ...)`, so the `ON` predicate cannot
see `l`. The query:

```sql
SELECT l.id, r.id, p.id
FROM l, r JOIN p ON l.id = p.l_id;
```

fails with:

- 1054 / SQLSTATE `42S22`: `Unknown column 'l.id' in 'on clause'`

The all-explicit form is valid because the final `ON` predicate can see the
left join subtree:

```sql
SELECT l.id AS l_id, r.id AS r_id, p.id AS p_id
FROM l JOIN r JOIN p ON l.id = p.l_id
WHERE l.id = r.l_id
ORDER BY l.id, r.id, p.id;
```

It returns `(1,10,100)`, `(2,20,200)`, and `(2,21,200)`.

MySQL also supports parenthesized table-reference groups to override
precedence, for example `(l, r) JOIN p ON l.id = p.l_id`. MyLite's current
covered surface accepts parenthesized nested join operands, such as
`l JOIN (r JOIN p ON ...) ON ...`, while broader comma-list parenthesized
groups remain deferred.

`ON` is not valid directly after a comma-separated table reference. The query
`SELECT * FROM l, r ON l.id = r.l_id` is a syntax error:

- 1064 / SQLSTATE `42000`

### Metadata

Result metadata for joined base-table columns follows the existing Task 23
field descriptor model.

For `ON` joins, `SELECT *` emits all visible columns from the left row source
followed by all visible columns from the right row source. Duplicate labels are
preserved. In the fixture, `SELECT * FROM l JOIN r ON l.id = r.l_id LIMIT 0`
reports twelve fields: all five columns from `l`, then all seven columns from
`r`. The duplicate labels `id`, `name`, `shared`, and `nullable` are not
renamed.

For `USING` joins, unqualified wildcard expansion coalesces the `USING`
columns as described above. The coalesced column's metadata is taken from the
left operand's column on the verified runtime. For
`SELECT * FROM l JOIN r USING (shared) LIMIT 0`, the first field is:

```text
Field='shared', Database='mylite_task26_inner_joins',
Table='l', Org_table='l', Type=LONG, Collation=binary (63), Length=11
```

For explicit qualified columns, aliases change the client-visible field label
and visible table metadata but do not change origin metadata. For example,
`SELECT l.id AS l_id, r.id AS r_id FROM l JOIN r ON l.id = r.l_id LIMIT 0`
reports labels `l_id` and `r_id`, with `Org_table` values `l` and `r`.

Qualified wildcards preserve the named table's raw column set and metadata.
They do not remove or coalesce `USING` columns.

Invisible-column wildcard behavior should be inherited from Task 15: wildcard
expansion omits invisible columns, while explicit references can still select
them. Task 26 should add joined-table tests for that inherited rule.

### Warnings and side effects

Join predicates share the same warning lifecycle as other expression
evaluation contexts. Successful statements retain warnings for inspected
expressions. Failed statements should leave diagnostics consistent with the
existing expression and statement error model.

Observed warning probe:

```sql
SELECT COUNT(*) FROM warn_l JOIN warn_r ON warn_r.s = 1;
SHOW COUNT(*) WARNINGS;
SHOW WARNINGS;
```

The result count is `0`. MySQL reports two warnings:

| Code | Message |
| --- | --- |
| 1292 | `Truncated incorrect DOUBLE value: 'bad'` |
| 1292 | `Truncated incorrect DOUBLE value: '2x'` |

For this fixture, MySQL evaluates the supported `ON` predicate for each
candidate row pair and records conversion warnings for the right-table values.
MyLite should not push `ON` predicates into SQLite or reorder expression
evaluation unless rows, warnings, and errors remain MySQL-compatible for the
supported slice.

Read-only `SELECT` joins must not change affected rows, session last insert id,
transaction state, schema state, or table metadata.

### Deferred observed surfaces

MySQL supports several join surfaces that Task 26 intentionally leaves for
later work:

| Surface | MySQL behavior observed or documented | Task 26 behavior |
| --- | --- | --- |
| `NATURAL JOIN` | Supported; joins on all common column names and coalesces common columns. | Deferred with deterministic unsupported parse/runtime diagnostic. |
| Parenthesized table references | Supported and can override comma/explicit join precedence. | Deferred. |
| Outer joins | Supported with null extension and different `ON`/`WHERE` placement effects. | Deferred to Task 27. |
| Derived tables and subqueries in `FROM` | Supported with required aliases for derived tables. | Deferred. |
| Index hints | Supported in table references. | Parsed and ignored by the placeholder slice. |
| Partitions | Supported in table references. | Deferred. |
| Multi-table `UPDATE`/`DELETE` joins | Supported by MySQL. | Deferred to DML-specific tasks. |

## MyLite parser and AST design

The current parser stores one table leaf in `MYLITE_SQL_AST_FROM_TABLE`. Task
26 needs a table-reference tree instead of a single table child. The tree must
preserve the syntactic grouping that matters for `ON` visibility and comma
precedence.

Recommended AST additions:

- `MYLITE_SQL_AST_FROM_TABLE_REFERENCES`, replacing or wrapping the current
  single-table `FROM` child for table-backed `SELECT`
- `MYLITE_SQL_AST_TABLE_REFERENCE_LIST`, representing comma-separated table
  references
- `MYLITE_SQL_AST_JOIN_EXPRESSION`, representing an explicit inner/cross join
- `MYLITE_SQL_AST_JOIN_CONDITION`, with variants for `ON` and `USING`
- `MYLITE_SQL_AST_USING_COLUMN_LIST`
- `MYLITE_SQL_AST_USING_COLUMN`

Recommended join operator enum:

```c
enum mylite_sql_ast_join_type {
    MYLITE_SQL_AST_JOIN_INNER,
    MYLITE_SQL_AST_JOIN_CROSS,
    MYLITE_SQL_AST_JOIN_COMMA,
};
```

The existing `MYLITE_SQL_AST_FROM_TABLE` node can remain the base-table leaf
and continue to store:

1. qualified table name
2. optional table alias

For an explicit join, the AST should store:

1. left table reference
2. right table reference
3. join type
4. optional join condition

For a comma table-reference list, the AST should preserve left-to-right order.
The analyzer may later lower comma items to Cartesian joins, but the parser
must not attach `ON` predicates to comma joins.

## Lemon-style grammar snippets

These snippets describe MyLite's intended Task 26 grammar shape. They are
independently authored and are not copied from MySQL grammar.

```lemon
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) table_references(C)
        opt_where_clause(D) opt_order_by_clause(E) opt_limit_clause(G). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table_references(state, F, C),
        D, NULL, NULL, E, G);
}

table_references(A) ::= comma_table_reference(B). {
    A = B;
}

comma_table_reference(A) ::= joined_table_reference(B). {
    A = B;
}
comma_table_reference(A) ::= comma_table_reference(B) COMMA joined_table_reference(C). {
    A = mylite_sql_parser_make_comma_join(state, B, C);
}

joined_table_reference(A) ::= table_factor(B). {
    A = B;
}
joined_table_reference(A) ::= joined_table_reference(B) inner_join_operator(C)
        table_factor(D) opt_inner_join_condition(E). {
    A = mylite_sql_parser_make_inner_join(state, B, C, D, E);
}

inner_join_operator(A) ::= JOIN(T). {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_INNER);
}
inner_join_operator(A) ::= INNER(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_INNER);
}
inner_join_operator(A) ::= CROSS(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_CROSS);
}

opt_inner_join_condition(A) ::= . {
    A = NULL;
}
opt_inner_join_condition(A) ::= ON(T) expression(B). {
    A = mylite_sql_parser_make_join_on_condition(state, T, B);
}
opt_inner_join_condition(A) ::= USING(T) LPAREN using_column_list(B) RPAREN. {
    A = mylite_sql_parser_make_join_using_condition(state, T, B);
}

using_column_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_using_column_list(state, B);
}
using_column_list(A) ::= using_column_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_using_column(state, B, C);
}

table_factor(A) ::= table_name(B) opt_table_alias(C). {
    A = mylite_sql_parser_make_from_table(state, B, C);
}
```

The split between `comma_table_reference` and `joined_table_reference` is what
gives explicit joins higher precedence than comma joins.

Deferred grammar must remain rejected or produce explicit unsupported
diagnostics:

```lemon
table_factor ::= LPAREN joined_table_reference RPAREN.

/* Deferred: parenthesized comma-list table-reference groups. */
table_factor ::= LPAREN table_references RPAREN.

/* Deferred: outer, natural, and straight joins. */
joined_table_reference ::= joined_table_reference LEFT JOIN table_factor join_condition.
joined_table_reference ::= joined_table_reference RIGHT JOIN table_factor join_condition.
joined_table_reference ::= joined_table_reference NATURAL JOIN table_factor.
joined_table_reference ::= joined_table_reference STRAIGHT_JOIN table_factor opt_join_condition.

/* Deferred: derived tables, lateral tables, and partitions. */
table_factor ::= table_subquery table_alias.
table_factor ::= LATERAL table_subquery table_alias.
table_factor ::= table_name PARTITION LPAREN identifier_list RPAREN opt_table_alias.
table_factor ::= table_name opt_table_alias index_hint_list.
```

If the existing grammar keeps separate `SELECT STAR` productions, Task 26
should update those productions to use the same `table_references` nonterminal
so wildcard expansion and join binding share one path.

## Runtime design

### Binding and table scope

Preparing a joined `SELECT` should build a statement-owned table scope from the
table-reference tree:

1. Resolve each base table against the selected schema or explicit schema.
2. Load table and column catalog rows in ordinal order.
3. Assign each base table a stable internal table slot.
4. Choose the visible qualifier: alias when present, otherwise base table
   name.
5. Reject duplicate visible qualifiers with MySQL's 1066-style
   not-unique-table-alias diagnostic.
6. Keep origin schema/table/column identity separate from visible alias
   metadata.

The resolver should retain MyLite's existing policy: schema/table/alias
qualifiers are byte-preserving and case-sensitive, while column-name lookup is
case-insensitive.

The analyzer should enforce MySQL's 61-table join limit when that limit is
reachable. If the first implementation chooses a lower temporary internal
limit, that limit must be documented as an explicit reduced-fidelity gap and
covered by tests. Prefer implementing the MySQL limit directly because the row
source is already statement-owned.

### Column resolution

Column resolution must be clause aware because diagnostics include the clause
name:

- select list: `field list`
- `ON`: `on clause`
- `WHERE`: `where clause`
- `ORDER BY`: `order clause`

Unqualified lookup:

- one matching visible column resolves
- no matches fail with 1054 / `42S22`
- two or more matches fail with 1052 / `23000`

Qualified lookup:

- `alias.column` resolves only through the visible alias
- `table.column` resolves through the base table name only when no alias hides
  it
- `schema.table.column` resolves through the schema-qualified base identity
  only when no alias hides it
- unknown or hidden qualifiers fail as unknown columns in the current clause

`USING` coalesced columns add one unqualified output column for wildcard and
unqualified column lookup. Qualified lookup must still be able to resolve each
base column, because MySQL permits qualified references to common `USING`
columns.

### Join evaluation

The first runtime can use a MyLite-owned nested-loop row source. This is simple
and preserves MySQL-visible warning and error behavior while the compatibility
surface is still small. SQLite pushdown can be added later only after tests
prove that it preserves rows, warning counts, diagnostics, metadata, and
statement side effects.

Evaluation order:

1. Produce the left row source.
2. For each left row, scan the right row source.
3. For a comma join or explicit join without condition, emit every row pair.
4. For an `ON` condition, evaluate the predicate in a scope containing only
   the explicit join's operands. Emit the pair only when the result is true.
5. For a `USING` condition, evaluate equality for each deduplicated `USING`
   column pair. Emit the pair only when every comparison is true.
6. After the complete `FROM` row is produced, evaluate `WHERE`.
7. Apply `ORDER BY`.
8. Apply `LIMIT` / `OFFSET`.

For inner joins, the optimizer may eventually reorder joins if and only if
MyLite preserves MySQL-compatible rows, metadata, warnings, and errors. The
first slice should keep source order to reduce risk.

### `USING` output columns

Each table reference should expose an ordered output-column list for wildcard
expansion. For a base table, that list is its visible columns in catalog order.
For an `ON` join or Cartesian join, the list is left output columns followed by
right output columns. For a `USING` join:

1. Validate that every listed column exists in both operands.
2. Deduplicate repeated `USING` names case-insensitively.
3. Build the coalesced-column set.
4. Emit coalesced columns first, ordered by their position in the left
   operand's output-column list.
5. Emit left output columns that are not coalesced.
6. Emit right output columns that are not coalesced.

For an inner join, the coalesced value can be read from the left operand after
the equality predicate succeeds. The descriptor should use the left operand's
metadata for the coalesced column. This matches the verified MySQL metadata for
`SELECT * FROM l JOIN r USING (shared) LIMIT 0`.

Qualified wildcard expansion must bypass the joined output list and use the
named base table's visible columns. This preserves both physical columns in
`SELECT l.*, r.* FROM l JOIN r USING (shared)`.

### Result metadata

Task 26 should reuse the Task 23 descriptor model and extend row-source binding
so each projected column has:

- visible label
- catalog `def`
- origin schema
- visible table name or alias
- origin table name
- origin column name
- type, length, decimals, charset, flags, and nullability from the base column
  or expression descriptor

Aliases affect the visible label and visible table name only. They must not
change origin metadata.

Duplicate output labels are valid. MyLite must not disambiguate labels for
client convenience.

### Unsupported shapes

Unsupported join shapes should fail before execution with deterministic
diagnostics. Prefer syntax rejection for grammar not yet accepted. If a shape
is parsed as part of the future full grammar but not executable yet, return a
clear unsupported-feature diagnostic without executing a partial query.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations.

### Successful rows

| Behavior | Expected outcome |
| --- | --- |
| `l INNER JOIN r ON l.id = r.l_id` | Returns `(1,10)`, `(2,20)`, `(2,21)` with explicit ordering. |
| `l JOIN r` without condition | Returns a Cartesian product; fixture count `12`. |
| `l CROSS JOIN r` without condition | Returns a Cartesian product; fixture count `12`. |
| `l CROSS JOIN r ON l.id = r.l_id` | Same rows as the equivalent inner `ON` join. |
| `l, r WHERE l.id = r.l_id` | Same rows as the equivalent inner join when filtered in `WHERE`. |
| `ON l.nullable = r.nullable` | Does not match `NULL` to `NULL`; fixture rows are `(2,20)`, `(2,40)`, `(3,20)`, `(3,40)`. |
| `WHERE` after join | Filters joined rows after `ON`; fixture `score >= 10 ORDER BY score DESC LIMIT 2` returns `(2,21,25)`, `(2,20,15)`. |
| all-explicit mixed join `l JOIN r JOIN p ON l.id = p.l_id WHERE l.id = r.l_id` | Returns `(1,10,100)`, `(2,20,200)`, `(2,21,200)`. |

### `USING` and wildcard metadata

| Behavior | Expected outcome |
| --- | --- |
| `SELECT * FROM l JOIN r ON l.id = r.l_id LIMIT 0` | Metadata lists all `l` visible columns, then all `r` visible columns; duplicate labels preserved. |
| `SELECT * FROM l JOIN r USING (shared)` | Rows match `shared` equality and output one coalesced `shared` column. |
| `SELECT * FROM l JOIN r USING (shared) LIMIT 0` | First metadata field is `shared` from table `l`, origin table `l`, type `LONG`. |
| `SELECT * FROM l JOIN r USING (shared, id) LIMIT 0` | Coalesced fields are `id`, then `shared`, following left-table order. |
| `SELECT * FROM l JOIN r USING (shared, shared)` | Accepted; same result shape as `USING (shared)`. |
| `SELECT shared, l.shared, r.shared FROM l JOIN r USING (shared)` | Unqualified `shared` resolves to the coalesced column; qualified forms resolve to base columns. |
| `SELECT l.*, r.* FROM l JOIN r USING (shared) LIMIT 0` | Metadata exposes all `l` columns followed by all `r` columns, including both `shared` columns. |
| `SELECT l.*, r.* FROM l JOIN r USING (shared) ORDER BY shared` | Fails with ambiguous `shared` in `order clause`. |

### Diagnostics

| Behavior | Expected diagnostic |
| --- | --- |
| ambiguous unqualified select column | 1052 / `23000`, `Column 'id' in field list is ambiguous` |
| ambiguous unqualified `WHERE` column | 1052 / `23000`, `Column 'id' in where clause is ambiguous` |
| alias-hidden base qualifier in select list | 1054 / `42S22`, `Unknown column 'l.id' in 'field list'` |
| alias-hidden base qualifier in `ON` | 1054 / `42S22`, `Unknown column 'l.id' in 'on clause'` |
| duplicate visible table alias | 1066 / `42000`, `Not unique table/alias: 'x'` |
| missing `USING` column | 1054 / `42S22`, `Unknown column 'missing_col' in 'from clause'` |
| `ON` references a later table | 1054 / `42S22`, unknown column in `on clause` |
| comma/explicit precedence hides comma-left table from `ON` | 1054 / `42S22`, unknown column in `on clause` |
| `ON` directly after comma table reference | 1064 / `42000` syntax error |

### Warnings and metadata

| Behavior | Expected outcome |
| --- | --- |
| `ON` predicate string-to-number conversion over `warn_l JOIN warn_r` | Count result `0`; two 1292 warnings for `'bad'` and `'2x'`. |
| joined `SELECT` metadata with table aliases | Visible `table` metadata uses aliases; `org_table` keeps base table names. |
| read-only side effects | Affected rows, last insert id, schema selection, and table metadata unchanged. |

## Test plan

Parser tests:

- base table comma list
- `JOIN`, `INNER JOIN`, and `CROSS JOIN` without conditions
- `JOIN`, `INNER JOIN`, and `CROSS JOIN` with `ON`
- `JOIN`, `INNER JOIN`, and `CROSS JOIN` with `USING`
- repeated `USING` names
- chained explicit joins
- parenthesized nested join operands
- mixed comma and explicit joins preserving explicit-join precedence
- aliases with and without `AS`
- qualified wildcards over joined table references
- syntax rejection for `ON` after comma joins
- rejection or unsupported handling for parenthesized comma-list table
  references, outer joins, natural joins, straight joins, derived tables,
  partitions, and index hints

Analyzer and runtime tests:

- selected-schema and schema-qualified resolution for every joined base table
- duplicate table/alias diagnostics
- alias-hidden base-name diagnostics in select list, `ON`, `WHERE`, and
  `ORDER BY`
- unqualified column success when exactly one joined table has the column
- ambiguous unqualified column diagnostics in select list, `ON`, `WHERE`, and
  `ORDER BY`
- `ON` predicate row filtering and three-valued logic
- `ON` conversion warnings and warning-count preservation
- `ON` operand visibility, including later-table rejection
- comma join Cartesian products
- explicit joins without conditions as Cartesian products
- comma/explicit precedence error cases
- `WHERE` filtering after join predicate evaluation
- `ORDER BY` over joined columns, projection aliases, ordinals, and hidden
  sort keys
- `LIMIT 0` metadata preservation for joined result sets
- `USING` row filtering
- `USING` missing-column diagnostics
- `USING` duplicate-column behavior
- `USING` coalesced wildcard output order
- qualified references to `USING` columns
- qualified wildcard expansion that bypasses `USING` coalescing
- invisible-column wildcard omission across joined base tables
- read-only statement side effects

Compatibility tests must compare against MySQL 8.4.9 for rows, numeric error
codes, SQLSTATE values, warning codes/messages, column labels, visible
table names, origin table names, origin column names, field types, flags,
lengths, decimals, charsets, nullability, and statement side effects.

## Compatibility decisions

- The first slice implements only inner semantics. Outer join syntax and
  execution stay unsupported until Task 27.
- Comma joins are supported because legacy MySQL application SQL still uses
  them, but explicit joins should be preferred in new MyLite tests when comma
  precedence is not the behavior under test.
- Parenthesized table-reference groups are deferred even though MySQL supports
  them. The explicit-join rewrite covers the common precedence workaround for
  the first slice.
- The first runtime should use MyLite-owned nested loops rather than SQLite
  join pushdown. Correct rows, warnings, and metadata are more important than
  early optimizer work.
- `USING` coalesced metadata uses the left operand's column descriptor for
  inner joins, matching observed MySQL behavior.
- Duplicate `USING` names are accepted and deduplicated after validation,
  matching observed MySQL behavior.
- Broad collation-sensitive join comparisons follow the existing expression
  compatibility envelope and remain a documented risk until collation work is
  wider.

## Implementation status

The Task 26 executable slice implements parser/AST coverage, ordered table
binding, duplicate alias checks, operand-scoped `ON` diagnostics, nested-loop
row production over SQLite row sources, `USING` validation and coalesced
wildcard output, projection/`WHERE`/`ORDER BY`/`LIMIT` integration, aggregate
scan compatibility for the supported aggregate subset, and runtime tests for
rows, metadata, warnings, and errors.

The nested-loop runtime loads each base table as a separate row source and
evaluates explicit join conditions as soon as the right operand is available.
This preserves MySQL-visible warning counts for staged joins and avoids
evaluating `ON` predicates against rows from later comma or explicit join
operands.

Grouped joined queries with `GROUP BY` or `HAVING` use the same joined row scan
as ungrouped aggregate calls, then apply the shared aggregate/grouping runtime.
Group reference resolution and `ONLY_FULL_GROUP_BY` validation are plan-wide.

The deferred surfaces listed above remain intentionally outside this task and
must not be inferred from the implemented base-table inner join slice.
