# ORDER BY, LIMIT, and OFFSET

## Scope

This feature specifies Task 18, adding deterministic result ordering and row
windowing to the single-table `SELECT` path implemented by Tasks 15 through 17.
It is a statement feature over existing expression and row-evaluation
machinery; it does not make joins, grouping, broad projection expressions, or
prepared statements supported.

In scope:

- `SELECT select_list FROM table_name [alias] [WHERE expression] ORDER BY ...`
- `SELECT select_list FROM table_name [alias] [WHERE expression] LIMIT ...`
- `ORDER BY` after the optional `WHERE` clause and before `LIMIT`
- one user base table only, with selected-schema and schema-qualified table
  resolution inherited from Task 15
- table aliases using `AS alias` or a bare identifier alias
- ordering by unqualified, table-qualified, alias-qualified, and
  schema-table-qualified columns
- ordering by projection aliases and positive select-list ordinals
- ordering by expressions over the Task 16 scalar operator subset, including
  hidden sort expressions that are not projected
- multiple sort keys, default ascending order, `ASC`, and `DESC`
- MySQL `NULL` placement for ascending and descending sorts
- string ordering through the current character-set/collation foundation for
  supported column and literal collations
- `LIMIT row_count`
- `LIMIT offset, row_count`
- `LIMIT row_count OFFSET offset`
- literal `LIMIT` bounds as unsigned integer constants in MySQL's accepted
  range
- design hooks for prepared-statement parameter markers in `LIMIT` bounds,
  while prepared statements remain deferred
- result metadata preservation, warning behavior for order expressions, and
  read-only side effects

Out of scope:

- multi-table queries, joins, derived tables, CTEs, table functions, lateral
  references, and parenthesized query expressions
- `ORDER BY` or `LIMIT` in `UPDATE`, `DELETE`, `TABLE`, `VALUES`, `UNION`, or
  subqueries
- `GROUP BY`, `HAVING`, `WITH ROLLUP`, window clauses, aggregate functions,
  `DISTINCT`, locking clauses, and `SELECT ... INTO`
- no-table scalar `SELECT` ordering/limiting. A later
  `INSERT ... SELECT FROM DUAL` slice added the covered scalar
  `FROM DUAL ORDER BY ... LIMIT` subset for standalone DUAL selects and
  insert-from-DUAL sources.
- arbitrary table-backed projection expressions and their result metadata,
  except where an expression is used only as a hidden sort key
- functions, casts, `CASE`, variables, user-defined variables, subqueries,
  row constructors, collations in expression syntax, regular expressions, JSON
  operators, and assignment operators
- explicit `COLLATE`, `BINARY expr`, non-ASCII collation equivalence,
  full collation coercibility, PAD SPACE / NO PAD edge cases, and
  `max_sort_length` behavior beyond the documented compatibility risk
- `NULLS FIRST` and `NULLS LAST`; MySQL 8.4 does not support those clauses in
  this feature surface
- optimizer-specific behavior such as index choice, filesort internals,
  stable tie ordering, early sort termination, and `FOUND_ROWS()`
- general prepared statement support; this spec records the future MySQL target
  behavior for `LIMIT ?` but Task 42 owns `PREPARE` and `EXECUTE`

Task 18 should not mark broader result metadata, table-backed projection
expressions, joins, grouping, or mutation statements as supported.

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Problems with Column Aliases:
  https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html
- MySQL 8.4 Reference Manual, `ORDER BY` optimization:
  https://dev.mysql.com/doc/refman/8.4/en/order-by-optimization.html
- MySQL 8.4 Reference Manual, `LIMIT` optimization:
  https://dev.mysql.com/doc/refman/8.4/en/limit-optimization.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Character Sets and Collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset.html
- MySQL 8.4 Reference Manual, The Binary Character Set:
  https://dev.mysql.com/doc/refman/8.4/en/charset-binary-set.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/character-set-collation-foundation/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.
  Metadata observations used `mysql --column-type-info -vvv`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Test fixture used for runtime verification

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_task18_order_limit;
CREATE DATABASE mylite_task18_order_limit;
USE mylite_task18_order_limit;

CREATE TABLE t (
  id INT PRIMARY KEY,
  category INT,
  n INT,
  s VARCHAR(20),
  nullable INT NULL,
  CamelCase INT
);

INSERT INTO t VALUES
  (1, 2, 10, 'beta', NULL, 100),
  (2, 1, 20, 'alpha', 5, 200),
  (3, 2, 10, 'alpha', NULL, 300),
  (4, 1, NULL, 'gamma', 0, 400),
  (5, 3, -1, 'Beta', 7, 500);
```

When examples use all necessary sort keys, the order is a MySQL compatibility
expectation. When two rows compare equal on every `ORDER BY` key, MySQL does
not guarantee their relative order, and tests must not depend on it.

### Clause order and execution position

For the Task 18 subset, `ORDER BY` follows `WHERE` and precedes `LIMIT`.
Filtering happens before ordering. `LIMIT` is applied to the ordered result.

Representative runtime result:

| SQL | Result row ids |
| --- | --- |
| `SELECT id FROM t WHERE category IN (1,2) ORDER BY category, id DESC LIMIT 3` | `4`, `2`, `3` |

### Sort keys and directions

Each `ORDER BY` item is a sort expression with an optional direction. The
default direction is ascending. Multiple sort keys are evaluated
lexicographically from left to right.

Representative runtime results:

| SQL | Result row ids |
| --- | --- |
| `SELECT id FROM t ORDER BY n ASC, id ASC` | `4`, `5`, `1`, `3`, `2` |
| `SELECT id FROM t ORDER BY n DESC, id ASC` | `2`, `1`, `3`, `5`, `4` |
| `SELECT id FROM t ORDER BY category ASC, s DESC, id ASC` | `4`, `2`, `1`, `3`, `5` |
| `SELECT id, n FROM t ORDER BY n + id DESC, id LIMIT 3` | `2`, `3`, `1` |

`NULL` values sort before non-`NULL` values in ascending order and after
non-`NULL` values in descending order:

| SQL | Result row ids |
| --- | --- |
| `SELECT id FROM t ORDER BY nullable ASC, id` | `1`, `3`, `4`, `2`, `5` |
| `SELECT id FROM t ORDER BY nullable DESC, id` | `5`, `2`, `4`, `1`, `3` |

String ordering follows MySQL collation rules for the expression's resolved
collation. Under the verified table default, `latin1_swedish_ci` compares the
ASCII examples case-insensitively, so a secondary key is needed for a fully
deterministic test:

| SQL | Result row ids |
| --- | --- |
| `SELECT id FROM t ORDER BY s ASC, id DESC` | `3`, `2`, `5`, `1`, `4` |

### Projection aliases, ordinals, and column resolution

Unqualified identifiers in `ORDER BY` search projection expressions first, then
table columns from the `FROM` clause. This differs from `WHERE`, where
projection aliases are not visible.

Representative runtime results:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT n AS sort_key, id FROM t ORDER BY sort_key DESC, id` | orders by the projected alias: ids `2`, `1`, `3`, `5`, `4` |
| `SELECT -id AS id, s FROM t ORDER BY id LIMIT 3` | alias `id` wins over base column `id`: projected values `-5`, `-4`, `-3` |
| `SELECT -n AS n, id FROM t ORDER BY t.n ASC, id LIMIT 3` | qualified `t.n` names the base column: ids `4`, `5`, `1` |
| `SELECT n AS Sort_Key, id FROM t ORDER BY sort_key DESC, id LIMIT 3` | alias lookup is case-insensitive for this verified runtime: ids `2`, `1`, `3` |
| ``SELECT n AS 'sort key', id FROM t ORDER BY `sort key` DESC, id`` | identifier-quoted reference resolves the quoted alias |
| `SELECT n AS 'sort key', id FROM t ORDER BY 'sort key' DESC, id LIMIT 3` | string literal is a constant sort key; secondary `id` controls the result |

Projection aliases are also visible inside a supported `ORDER BY` expression:

| SQL | Result row ids |
| --- | --- |
| `SELECT n AS x, id FROM t ORDER BY x + 1 DESC LIMIT 2` | `2`, `1` |
| `SELECT n AS x, id FROM t ORDER BY (x) DESC LIMIT 2` | `2`, `1` |
| `SELECT n AS x, id FROM t ORDER BY t.x` | error 1054 / `42S22`, unknown column `t.x` in `order clause` |

Duplicate projection aliases are accepted in the select list, but an
unqualified `ORDER BY` reference to that duplicate alias is ambiguous:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT n AS x, category AS x FROM t ORDER BY x LIMIT 1` | error 1052 / `23000`, `Column 'x' in order clause is ambiguous` |

Positive integer literals in a top-level `ORDER BY` item are one-based
select-list ordinals:

| SQL | Result row ids |
| --- | --- |
| `SELECT s, id FROM t ORDER BY 1 ASC, 2 DESC` | `3`, `2`, `5`, `1`, `4` |

Ordinal `0` and ordinals larger than the select-list width fail as unknown
columns in the order clause. A signed negative value is an expression constant,
not an ordinal.

| SQL | MySQL behavior |
| --- | --- |
| `SELECT id FROM t ORDER BY 0` | error 1054 / `42S22`, unknown column `0` in `order clause` |
| `SELECT id FROM t ORDER BY 2` | error 1054 / `42S22`, unknown column `2` in `order clause` |
| `SELECT id FROM t ORDER BY -1 LIMIT 2` | accepted; `-1` is a constant sort expression |

Qualified column references use the same visible table identity as Task 15 and
Task 17. When a table alias is present, the alias hides the base table name.

| SQL | MySQL behavior |
| --- | --- |
| `SELECT id FROM t AS tt ORDER BY tt.CamelCase DESC LIMIT 3` | returns ids `5`, `4`, `3` |
| `SELECT id FROM t AS tt ORDER BY t.n` | error 1054 / `42S22`, unknown column `t.n` in `order clause` |
| `SELECT id FROM t ORDER BY missing_col` | error 1054 / `42S22`, unknown column `missing_col` in `order clause` |
| `SELECT id FROM t ORDER BY missing_alias.n` | error 1054 / `42S22`, unknown column `missing_alias.n` in `order clause` |

On the verified Linux MySQL runtime, schema, table, and alias qualifiers are
case-sensitive while column and projection-alias lookup is case-insensitive.
This matches the existing MyLite policy direction from Tasks 15 and 17.

### LIMIT and OFFSET forms

MySQL supports three equivalent row-window forms in this feature surface:

- `LIMIT row_count`
- `LIMIT offset, row_count`
- `LIMIT row_count OFFSET offset`

The comma form lists offset first, then row count. The `OFFSET` keyword form
lists row count first, then offset. The initial row has offset `0`.

Representative runtime results:

| SQL | Result row ids |
| --- | --- |
| `SELECT id FROM t ORDER BY id LIMIT 2` | `1`, `2` |
| `SELECT id FROM t ORDER BY id LIMIT 1,2` | `2`, `3` |
| `SELECT id FROM t ORDER BY id LIMIT 2 OFFSET 1` | `2`, `3` |
| `SELECT id FROM t ORDER BY id LIMIT 0` | empty result, with result metadata still available |
| `SELECT id FROM t ORDER BY id LIMIT 2,18446744073709551615` | `3`, `4`, `5` |

Literal `LIMIT` arguments are parsed as unquoted nonnegative unsigned integer
constants. The largest accepted literal value observed for row count is
`18446744073709551615`.

The following literal forms are syntax errors in MySQL 8.4.9:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT id FROM t ORDER BY id LIMIT -1` | error 1064 / `42000` near `-1` |
| `SELECT id FROM t ORDER BY id LIMIT 1.5` | error 1064 / `42000` near `1.5` |
| `SELECT id FROM t ORDER BY id LIMIT '2'` | error 1064 / `42000` near `'2'` |
| `SELECT id FROM t ORDER BY id LIMIT NULL` | error 1064 / `42000` near `NULL` |
| `SELECT id FROM t ORDER BY id LIMIT 1,-2` | error 1064 / `42000` near `-2` |
| `SELECT id FROM t ORDER BY id LIMIT 2 OFFSET -1` | error 1064 / `42000` near `-1` |
| `SELECT id FROM t ORDER BY id LIMIT 1+1` | error 1064 / `42000` near `+1` |
| `SELECT id FROM t LIMIT 18446744073709551616` | error 1064 / `42000` near the too-large literal |

### Parameter markers in LIMIT

MySQL accepts `?` parameter markers in `LIMIT` bounds only inside prepared
statements. Outside a prepared statement, `LIMIT ?` is a syntax error.

Future prepared-statement support should match these observed MySQL 8.4.9
execute-time behaviors:

| Prepared SQL and bound value | MySQL behavior |
| --- | --- |
| `LIMIT ?`, bound integer `2` | returns first two ordered rows |
| `LIMIT ?`, bound string `'2'` | error 1210 / `HY000`, `Incorrect arguments to EXECUTE` |
| `LIMIT ?`, bound decimal `2.9` | error 1210 / `HY000`, `Incorrect arguments to EXECUTE` |
| `LIMIT ?`, bound integer `-1` | error 1690 / `22003`, unsigned integer out of range in `EXECUTE` |
| `LIMIT ?`, bound `NULL` | accepted and returns zero rows |
| `LIMIT 2 OFFSET ?`, bound `NULL` | accepted and treats the offset as `0` |
| `LIMIT ?, ?`, bound integers `1`, `2` | returns rows at offsets 1 and 2 |

Task 18 should preserve enough AST information to support this later, but it
does not need to implement `PREPARE`, `EXECUTE`, binary protocol binding, or
parameter-marker evaluation.

### Warnings, metadata, and side effects

`ORDER BY` expressions are evaluated for rows that survive `WHERE` before
`LIMIT` is applied. Conversion warnings from those expressions are visible even
when the row is not returned because of `LIMIT`.

Representative runtime results:

| SQL | Result row ids | Warnings |
| --- | --- | --- |
| `SELECT id FROM t ORDER BY s + 0, id LIMIT 2` | `1`, `2` | five 1292 warnings, one for each `s` value |
| `SELECT id FROM t WHERE category = 1 ORDER BY s + 0, id LIMIT 2` | `2`, `4` | two 1292 warnings, one for each filtered row |

`ORDER BY` and `LIMIT` do not add result columns and do not change projection
metadata. Metadata belongs to the projected output columns, not hidden sort
keys. Verified with `mysql --column-type-info -vvv`:

```sql
SELECT n AS x, s
FROM t
WHERE category IN (1,2)
ORDER BY nullable DESC, id
LIMIT 2;
```

The output fields are `x` and `s`; both retain schema, table, origin table,
type, length, collation, decimals, and flags from their projected columns.
The `nullable` and `id` sort keys are not exposed as result fields.

The statement is read-only:

| SQL sequence | MySQL behavior |
| --- | --- |
| insert one row into an auto-increment table, then `SELECT ... ORDER BY ... LIMIT ...` | `LAST_INSERT_ID()` remains the inserted id |
| `ROW_COUNT()` after a result-producing `SELECT ... ORDER BY ... LIMIT ...` | returns `-1` |
| successful `SELECT ... ORDER BY ... LIMIT ...` with no warning-producing expression | warning count remains `0` |

## MyLite behavior

### Parser and AST

The parser should extend the existing `SELECT` statement shape with optional
`ORDER BY` and `LIMIT` clauses after the optional `WHERE` clause. The intended
child order for table-backed statements is:

1. `MYLITE_SQL_AST_SELECT_LIST`
2. `MYLITE_SQL_AST_FROM_TABLE`
3. optional `MYLITE_SQL_AST_WHERE_CLAUSE`
4. optional `MYLITE_SQL_AST_ORDER_BY_CLAUSE`
5. optional `MYLITE_SQL_AST_LIMIT_CLAUSE`

Recommended AST additions:

- `MYLITE_SQL_AST_ORDER_BY_CLAUSE`
- `MYLITE_SQL_AST_ORDER_ITEM_LIST`
- `MYLITE_SQL_AST_ORDER_ITEM`, with one child expression and an explicit
  direction enum or flag
- `MYLITE_SQL_AST_LIMIT_CLAUSE`, storing row-count and optional offset children
  in semantic order, regardless of the SQL spelling
- `MYLITE_SQL_AST_LIMIT_BOUND`, storing the literal token spelling and parsed
  unsigned value, or a parameter-marker placeholder for future prepared
  statements

The `LIMIT` AST should normalize these forms:

- `LIMIT row_count` -> offset `0`, row count `row_count`
- `LIMIT offset, row_count` -> offset `offset`, row count `row_count`
- `LIMIT row_count OFFSET offset` -> offset `offset`, row count `row_count`

Literal limit values should be validated during parsing or prepare so invalid
literal forms produce syntax diagnostics before execution. The parser must
preserve source spans for the whole clause and for individual sort keys and
limit bounds.

### Lemon grammar snippets

These snippets describe MyLite's intended Task 18 grammar shape. They are not
copied from MySQL grammar.

```lemon
select_statement ::= SELECT select_item_list FROM table_name opt_table_alias
    opt_where_clause opt_order_by_clause opt_limit_clause.
select_statement ::= SELECT STAR FROM table_name opt_table_alias
    opt_where_clause opt_order_by_clause opt_limit_clause.

opt_order_by_clause ::= .
opt_order_by_clause ::= order_by_clause.

order_by_clause ::= ORDER BY order_item_list.

order_item_list ::= order_item.
order_item_list ::= order_item_list COMMA order_item.

order_item ::= expression opt_order_direction.

opt_order_direction ::= .
opt_order_direction ::= ASC.
opt_order_direction ::= DESC.

opt_limit_clause ::= .
opt_limit_clause ::= limit_clause.

limit_clause ::= LIMIT limit_bound.
limit_clause ::= LIMIT limit_bound COMMA limit_bound.
limit_clause ::= LIMIT limit_bound OFFSET limit_bound.

limit_bound ::= unsigned_integer_literal.
limit_bound ::= PARAMETER_MARKER.
```

`ORDER`, `BY`, `LIMIT`, and `OFFSET` are clause keywords. Bare table aliases
must not consume `WHERE`, `ORDER`, or `LIMIT` as aliases. Bare projection
aliases must not consume `FROM` as an alias boundary. `PARAMETER_MARKER` in a
limit bound is accepted only in a prepared-statement parse context once Task 42
exists; direct text execution should match MySQL's syntax error for `LIMIT ?`.

The following MySQL-supported forms remain outside Task 18:

```lemon
/* Deferred: rowless SELECT ordering/limiting. */
select_statement ::= SELECT select_item_list opt_order_by_clause opt_limit_clause.

/* Deferred: grouping, post-group filtering, and windows. */
select_statement ::= SELECT select_item_list FROM table_reference where_clause
    GROUP BY group_list HAVING expression order_by_clause limit_clause.
select_statement ::= SELECT select_item_list FROM table_reference WINDOW window_list
    order_by_clause limit_clause.

/* Deferred: joins and broader table references. */
select_statement ::= SELECT select_item_list FROM table_reference_list
    opt_where_clause order_by_clause limit_clause.

/* Deferred: set operations and query expressions. */
query_expression ::= query_expression UNION query_expression order_by_clause
    limit_clause.

/* Deferred: mutation statement ordering and limiting. */
update_statement ::= UPDATE table_name SET assignment_list where_clause
    order_by_clause limit_clause.
delete_statement ::= DELETE FROM table_name where_clause order_by_clause
    limit_clause.
```

### ORDER BY binding

The `ORDER BY` binder should run after the select list and table reference are
resolved and after the optional `WHERE` predicate is bound.

Binding inputs:

- resolved table schema and base table name
- optional table alias
- catalog-ordered column metadata
- projection output list after wildcard expansion
- projection alias/label map, preserving duplicates
- diagnostic context string, `order clause`

Resolution rules:

- A top-level unsigned integer literal in an order item is a one-based
  projection ordinal.
- Ordinal `0` or an ordinal greater than the projection count fails with
  1054-style unknown-column diagnostics in `order clause`.
- Signed negative values such as `-1` are expression constants, not ordinals.
- Unqualified identifier references in an order expression first resolve
  against projection aliases and labels, then against table columns.
- If the unqualified name matches multiple projection outputs, fail with
  1052-style ambiguous-column diagnostics in `order clause`.
- If a projection alias and a table column have the same unqualified name, the
  projection alias wins.
- Identifier-quoted references may resolve projection aliases created with
  string-quoted aliases; string literal tokens remain string constants.
- Qualified references never resolve projection aliases. They resolve only
  table columns through the visible table identity.
- Two-part column references match the visible table identity and then the
  column. The visible table identity is the alias when present, otherwise the
  base table name.
- Three-part column references match `schema.table.column` only when no table
  alias is present and both schema and table match the resolved table.
- Schema, table, and alias qualifier comparisons use MyLite's existing
  case-sensitive policy. Column and projection-alias lookup should be
  case-insensitive for the supported identifier repertoire.
- Unknown order columns and unknown order qualifiers use the 1054-style
  unknown-column diagnostic with `in 'order clause'`.

Projection-alias sort keys should evaluate the projection value, not rebind a
same-named base column. For Task 18, this is required for direct column
projection aliases and should be designed to extend to arbitrary projection
expressions when that feature lands.

Hidden sort expressions that are not projected may use the Task 16 expression
subset over row columns, literals, and projection aliases. Unsupported
expression shapes should fail deterministically unless MySQL reports a syntax
or semantic error first.

### Runtime execution

The initial implementation should prefer MyLite-owned row sorting over
delegating the feature to SQLite `ORDER BY` and `LIMIT`. SQLite differs from
MySQL in name-resolution rules, accepted `LIMIT` expressions, negative limit
handling, type affinity, string comparison, collation coverage, warning
records, and expression evaluation diagnostics.

Recommended execution model:

1. Build the Task 15 single-table select plan.
2. Bind the optional Task 17 predicate.
3. Expand the projection list and build projection metadata.
4. Bind `ORDER BY` keys against projection outputs and the row context.
5. Validate and normalize `LIMIT` bounds.
6. Determine the union of physical columns needed by projection, predicate,
   and hidden order expressions.
7. Read physical rows from SQLite without relying on SQLite ordering for
   MySQL semantics.
8. Evaluate the `WHERE` predicate, keeping only true rows.
9. For each kept row, evaluate every order key in SQL order and append warning
   records in MySQL-observed order.
10. Sort kept rows by their MySQL order-key values.
11. Apply offset and row count.
12. Produce projection values and Task 15 result metadata for the selected
    window.

If there is no `ORDER BY`, `LIMIT` may be applied while streaming rows after
`WHERE` filtering. MyLite must not document or test a deterministic no-order
row sequence unless a later feature explicitly does so.

Sort comparison requirements:

- `ASC` is the default direction.
- `NULL` sorts before non-`NULL` for ascending keys and after non-`NULL` for
  descending keys.
- Integer, unsigned integer, decimal, floating, string, binary, and `NULL`
  sort keys should compare through the same value model used by Task 16.
- String keys should use the expression's resolved MySQL collation for the
  supported charset/collation subset.
- Equal sort keys do not imply a stable order. Tests must add a deterministic
  secondary key when row order matters.

`LIMIT` requirements:

- Store bounds as unsigned 64-bit values.
- Accept `18446744073709551615` as a literal bound.
- Reject larger literal bounds as syntax errors.
- Reject negative, decimal, quoted string, `NULL`, and arithmetic-expression
  literal bounds as syntax errors.
- Treat `LIMIT 0` as a valid empty result set with metadata.
- Apply offset before row count; offset is zero-based.
- Preserve parser state for future parameter-marker bounds, but do not execute
  markers until prepared statements are implemented.

### Diagnostics and warnings

Required target diagnostics for Task 18:

- 1052 / `23000`: `Column 'name' in order clause is ambiguous`
- 1054 / `42S22`: `Unknown column 'name' in 'order clause'`
- 1064 / `42000`: syntax errors for invalid literal `LIMIT` bounds, misplaced
  clauses, unsupported `NULLS FIRST` / `NULLS LAST`, and `LIMIT ?` outside a
  prepared statement
- 1210 / `HY000`: future prepared-statement execute-time error for noninteger
  `LIMIT` marker values
- 1292 / `22007`: conversion warnings from supported order expressions
- 1690 / `22003`: future prepared-statement execute-time unsigned range error
  for negative integer `LIMIT` marker values

Warnings from `WHERE` and `ORDER BY` expressions share the statement warning
area. `ORDER BY` expression warnings are emitted only for rows that pass
`WHERE`, and before `LIMIT` removes rows from the result window.

Current MyLite message-only errors should still use the MySQL wording where
the project has no numeric-code surface yet.

### Metadata and side effects

Task 18 does not add result metadata fields and does not change projected
field metadata:

- `mylite_column_name()` returns the projection label from Task 15.
- Schema, visible table, origin table, and origin column metadata come from
  projected output columns.
- Hidden order expressions and ordinal references do not appear in the result
  column list.
- `LIMIT 0` still exposes the same result metadata that the query would expose
  without the limit.
- Computed projection expression metadata remains deferred to Task 23.

The statement is read-only:

- no catalog mutation
- no physical table mutation
- no change to session last insert id
- no DML affected-row count; MySQL `ROW_COUNT()` after a result-producing
  `SELECT` reports `-1`

### Storage and performance implications

Task 18 does not change the `.mylite` file format, schema catalog, table
catalog, or column catalog. It adds statement-runtime state:

- bound order-key list
- normalized limit offset and row count
- optional future parameter-marker placeholders
- sort-key values for rows that pass `WHERE`
- warning records produced by order-expression evaluation
- scratch storage for collation keys and converted values

The first implementation may materialize all rows that pass `WHERE` before
sorting. That is acceptable for Task 18 because MySQL-compatible value
comparison, collation, warning timing, and diagnostics are the priority. Later
optimizer work may push simple order/limit operations to SQLite only when
tests prove that MySQL-visible behavior is preserved.

Avoid per-row heap churn for common integer, `NULL`, and short string sort
keys. If collation keys or converted values allocate, make their lifetime
statement-owned and clean them after the result row window has been produced.

## SQLite-vs-MySQL semantic risks

- SQLite accepts `LIMIT` expressions and has different behavior for negative
  or noninteger bounds; MyLite must parse and validate MySQL's literal forms
  itself.
- SQLite name resolution does not provide MySQL's `ORDER BY` rule of checking
  projection aliases before `FROM` columns, and it will not report MySQL's
  1052/1054 diagnostics.
- SQLite collations do not match MySQL collations beyond deliberately mapped
  subsets. MyLite must not rely on SQLite text ordering for MySQL-visible
  string sort semantics.
- SQLite expression evaluation would miss or reorder MySQL warning records,
  especially for string-to-number conversions in hidden sort expressions.
- SQLite row-value ordering, numeric affinity, and mixed-type comparison do not
  fully match MySQL's value model.
- SQLite may provide stable rowid tie behavior in cases where MySQL gives no
  order guarantee. MyLite tests must avoid asserting tie order unless all
  required tie-breaker keys are present.
- MySQL optimizer plans can change tie order when `LIMIT` is present. MyLite
  should match the guarantee, not a specific optimizer plan.
- MySQL's `max_sort_length`, PAD SPACE / NO PAD behavior, and broad Unicode
  collation weights are larger compatibility surfaces than the current
  character-set foundation. They should remain explicit deferred risks until
  implemented.

## Explicit deferred behavior

- General prepared statements and marker binding are deferred to Task 42.
- `ORDER BY` and `LIMIT` in mutation statements are deferred to Tasks 19 and
  20.
- Joins, ambiguous table-column references across multiple tables, and joined
  result ordering are deferred to join tasks.
- Grouped ordering, aggregate ordering, `HAVING`, windows, and rollup are
  deferred to grouping/window tasks.
- `DISTINCT` interaction with ordering and limits is deferred to Task 28.
- Set-operation ordering and limits are deferred to Task 30.
- Arbitrary projected expression metadata is deferred to Task 23.
- Full collation coercibility, explicit `COLLATE`, non-ASCII ordering,
  binary-string edge cases, PAD SPACE / NO PAD, and `max_sort_length` are
  deferred to collation and result-metadata work.
- Optimizer behavior, index ordering, filesort implementation details, and
  top-N early termination are deferred.
- `SQL_CALC_FOUND_ROWS` and `FOUND_ROWS()` are deferred.

## MySQL-runtime-verified test expectations

Implementation tests should compare MyLite against MySQL 8.4.9 for at least
these cases. Ordered result tests must include all tie-breaker keys needed to
make the order deterministic.

### Parser tests

| SQL | Expected parser outcome |
| --- | --- |
| `SELECT id FROM t ORDER BY id` | accepted with an order-by clause after the table reference |
| `SELECT id FROM t WHERE category = 1 ORDER BY id` | accepted with `WHERE` before `ORDER BY` |
| `SELECT id FROM t ORDER BY id LIMIT 2` | accepted with `ORDER BY` before `LIMIT` |
| `SELECT id FROM t LIMIT 2` | accepted with no `ORDER BY` |
| `SELECT id FROM t LIMIT 1,2` | accepted and normalized to offset `1`, row count `2` |
| `SELECT id FROM t LIMIT 2 OFFSET 1` | accepted and normalized to offset `1`, row count `2` |
| `SELECT id FROM t ORDER BY id ASC, category DESC` | accepted with two direction-bearing order items |
| `SELECT id FROM t ORDER BY n + id DESC` | accepted if the expression is within the Task 16 subset |
| `SELECT id FROM t LIMIT -1` | syntax error 1064 / `42000` |
| `SELECT id FROM t LIMIT 1.5` | syntax error 1064 / `42000` |
| `SELECT id FROM t LIMIT '2'` | syntax error 1064 / `42000` |
| `SELECT id FROM t LIMIT NULL` | syntax error 1064 / `42000` |
| `SELECT id FROM t LIMIT 1+1` | syntax error 1064 / `42000` |
| `SELECT id FROM t LIMIT ?` outside prepared context | syntax error 1064 / `42000` |
| `SELECT id FROM t ORDER BY id NULLS LAST` | syntax error 1064 / `42000`; MySQL does not support this clause |
| `SELECT id FROM t ORDER BY id WHERE category = 1` | syntax error because clause order is invalid |
| `SELECT id FROM t GROUP BY category ORDER BY id` | remains unsupported until grouping |
| `SELECT id FROM t JOIN t AS u ORDER BY t.id` | remains unsupported until joins |

### Basic ordering

| SQL | Expected result row ids |
| --- | --- |
| `SELECT id FROM t ORDER BY id` | `1`, `2`, `3`, `4`, `5` |
| `SELECT id FROM t ORDER BY id DESC` | `5`, `4`, `3`, `2`, `1` |
| `SELECT id FROM t ORDER BY n ASC, id ASC` | `4`, `5`, `1`, `3`, `2` |
| `SELECT id FROM t ORDER BY n DESC, id ASC` | `2`, `1`, `3`, `5`, `4` |
| `SELECT id FROM t ORDER BY category ASC, s DESC, id ASC` | `4`, `2`, `1`, `3`, `5` |
| `SELECT id FROM t ORDER BY s ASC, id DESC` | `3`, `2`, `5`, `1`, `4` |
| `SELECT id FROM t ORDER BY n + id DESC, id LIMIT 3` | `2`, `3`, `1` |

### Aliases, ordinals, and qualifiers

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `SELECT n AS sort_key, id FROM t ORDER BY sort_key DESC, id` | returns ids `2`, `1`, `3`, `5`, `4` |
| `SELECT -id AS id, s FROM t ORDER BY id LIMIT 3` | alias wins over base column; projected values are `-5`, `-4`, `-3` |
| `SELECT -n AS n, id FROM t ORDER BY t.n ASC, id LIMIT 3` | qualified base column wins; ids `4`, `5`, `1` |
| `SELECT n AS Sort_Key, id FROM t ORDER BY sort_key DESC, id LIMIT 3` | alias lookup is case-insensitive; ids `2`, `1`, `3` |
| ``SELECT n AS 'sort key', id FROM t ORDER BY `sort key` DESC, id`` | identifier-quoted alias reference sorts by the alias |
| `SELECT n AS 'sort key', id FROM t ORDER BY 'sort key' DESC, id LIMIT 3` | string literal is a constant; ids `1`, `2`, `3` because secondary `id` controls the result |
| `SELECT n AS x, id FROM t ORDER BY x + 1 DESC LIMIT 2` | alias is visible inside the order expression; ids `2`, `1` |
| `SELECT n AS x, category AS x FROM t ORDER BY x LIMIT 1` | error 1052 / `23000`, ambiguous `x` in `order clause` |
| `SELECT s, id FROM t ORDER BY 1 ASC, 2 DESC` | returns ids `3`, `2`, `5`, `1`, `4` |
| `SELECT id FROM t ORDER BY 0` | error 1054 / `42S22`, unknown column `0` in `order clause` |
| `SELECT id FROM t ORDER BY 2` | error 1054 / `42S22`, unknown column `2` in `order clause` |
| `SELECT id FROM t ORDER BY -1 LIMIT 2` | accepted; constant sort key, no ordinal lookup |
| `SELECT id FROM t AS tt ORDER BY tt.CamelCase DESC LIMIT 3` | returns ids `5`, `4`, `3` |
| `SELECT id FROM t AS tt ORDER BY t.n` | error 1054 / `42S22`, unknown column `t.n` in `order clause` |
| `SELECT id FROM t ORDER BY missing_col` | error 1054 / `42S22`, unknown column `missing_col` in `order clause` |
| `SELECT id FROM t ORDER BY missing_alias.n` | error 1054 / `42S22`, unknown column `missing_alias.n` in `order clause` |

### NULL ordering

| SQL | Expected result row ids |
| --- | --- |
| `SELECT id FROM t ORDER BY nullable ASC, id` | `1`, `3`, `4`, `2`, `5` |
| `SELECT id FROM t ORDER BY nullable DESC, id` | `5`, `2`, `4`, `1`, `3` |

### LIMIT and OFFSET

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `SELECT id FROM t ORDER BY id LIMIT 2` | ids `1`, `2` |
| `SELECT id FROM t ORDER BY id LIMIT 1,2` | ids `2`, `3` |
| `SELECT id FROM t ORDER BY id LIMIT 2 OFFSET 1` | ids `2`, `3` |
| `SELECT id FROM t ORDER BY id LIMIT 0` | empty result with unchanged output metadata |
| `SELECT id FROM t ORDER BY id LIMIT 2,18446744073709551615` | ids `3`, `4`, `5` |
| `SELECT id FROM t ORDER BY id LIMIT -1` | syntax error 1064 / `42000` |
| `SELECT id FROM t ORDER BY id LIMIT 1.5` | syntax error 1064 / `42000` |
| `SELECT id FROM t ORDER BY id LIMIT '2'` | syntax error 1064 / `42000` |
| `SELECT id FROM t ORDER BY id LIMIT NULL` | syntax error 1064 / `42000` |
| `SELECT id FROM t ORDER BY id LIMIT 1,-2` | syntax error 1064 / `42000` |
| `SELECT id FROM t ORDER BY id LIMIT 2 OFFSET -1` | syntax error 1064 / `42000` |
| `SELECT id FROM t LIMIT 18446744073709551616` | syntax error 1064 / `42000` |

### WHERE interaction and warning behavior

| SQL | Expected result row ids | Expected warnings |
| --- | --- | --- |
| `SELECT id FROM t WHERE category IN (1,2) ORDER BY category, id DESC LIMIT 3` | `4`, `2`, `3` | none |
| `SELECT id FROM t WHERE category = 1 ORDER BY s + 0, id LIMIT 2` | `2`, `4` | two 1292 warnings |
| `SELECT id FROM t ORDER BY s + 0, id LIMIT 2` | `1`, `2` | five 1292 warnings |
| `SELECT id FROM t WHERE category = 99 ORDER BY s + 0 LIMIT 2` | empty | no conversion warnings, because no rows pass `WHERE` |

### Prepared-marker target behavior

These expectations belong to the future prepared-statement test suite, but
Task 18 should keep the AST/runtime design compatible with them.

| Prepared SQL and bound value | Expected MyLite-compatible outcome |
| --- | --- |
| `PREPARE stmt FROM 'SELECT id FROM t ORDER BY id LIMIT ?'; EXECUTE stmt USING @row_count` with `@row_count := 2` | ids `1`, `2` |
| same statement with `@row_count := '2'` | error 1210 / `HY000`, incorrect arguments to `EXECUTE` |
| same statement with `@row_count := 2.9` | error 1210 / `HY000`, incorrect arguments to `EXECUTE` |
| same statement with `@row_count := -1` | error 1690 / `22003`, unsigned integer out of range in `EXECUTE` |
| same statement with `@row_count := NULL` | accepted and returns zero rows |
| `PREPARE stmt FROM 'SELECT id FROM t ORDER BY id LIMIT 2 OFFSET ?'` with `@skip := NULL` | accepted and returns ids `1`, `2` |
| `PREPARE stmt FROM 'SELECT id FROM t ORDER BY id LIMIT ?, ?'` with `@skip := 1`, `@take := 2` | ids `2`, `3` |

### Metadata and side effects

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `SELECT n AS x, s FROM t WHERE category IN (1,2) ORDER BY nullable DESC, id LIMIT 2` | output columns are only `x` and `s`; hidden sort keys are not exposed |
| same query with column metadata inspection | projected columns retain their Task 15 schema/table/origin/type metadata |
| `SELECT id FROM t ORDER BY id LIMIT 0` | no rows, but result metadata is available |
| successful `SELECT ... ORDER BY ... LIMIT ...` | read-only; no catalog or physical table mutation |
| auto-increment insert followed by `SELECT ... ORDER BY ... LIMIT ...` | `LAST_INSERT_ID()` remains the inserted id |
| `ROW_COUNT()` after result-producing `SELECT ... ORDER BY ... LIMIT ...` | returns `-1` |

## Implementation handoff notes

- Add parser and AST support first, with clause ordering tests before runtime
  changes.
- Normalize `LIMIT` bounds in the AST so runtime code does not care which SQL
  spelling was used.
- Keep `ORDER BY` binding separate from `WHERE` binding because alias
  visibility is different.
- Reuse the Task 16 value model and Task 17 row-expression context for hidden
  sort expressions.
- Sort in MyLite first. Do not use SQLite `ORDER BY` or `LIMIT` as the
  semantic authority until a later optimizer can prove specific cases safe.
- Add warning-order tests for sort expressions before adding any top-N or
  pushdown optimization.
- Use deterministic secondary keys in runtime tests. Equal sort keys have no
  MySQL row-order guarantee.
