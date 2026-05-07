# WHERE clause

## Scope

This feature specifies Task 17, `WHERE` predicate evaluation for the
table-backed `SELECT` subset that Task 15 made executable. It connects the
single-table SELECT plan to the Task 16 expression operator foundation, adding
row filtering, row-scoped column resolution, and MySQL-compatible warning and
diagnostic behavior for predicates.

In scope:

- `SELECT select_list FROM table_name [alias] WHERE expression`
- schema-qualified table names and selected-schema table resolution inherited
  from Task 15
- one user base table only
- table aliases using `AS alias` or a bare identifier alias
- unqualified, table-qualified, alias-qualified, and schema-table-qualified
  column references inside the predicate
- literal predicates, direct column truthiness, and predicates over the Task 16
  scalar operator subset
- three-valued filtering where only a true predicate keeps a row
- conversion warnings and predicate errors produced while evaluating each row
- warning-list and warning-count lifecycle for successful and failed predicate
  statements
- result metadata preservation for the projection list
- deterministic diagnostics for unknown predicate columns, alias-hidden base
  names, and unsupported predicate shapes

Out of scope:

- joins, comma table references, derived tables, table functions, lateral
  references, and parenthesized table references
- ambiguous unqualified predicate column diagnostics in multi-table queries;
  the MySQL behavior is documented here, but implementation belongs to the join
  tasks
- `WHERE` for `UPDATE`, `DELETE`, `SHOW`, subqueries, CTEs, views, and
  information-schema system views unless a later task explicitly widens them
- `SELECT` without `FROM` with `WHERE`; MySQL supports this form, but Task 17 is
  scoped to table-backed SELECT. A later `INSERT ... SELECT FROM DUAL` slice
  added the covered scalar `FROM DUAL WHERE` subset for no-table predicates.
- `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, locking clauses, `SELECT ... INTO`,
  and SELECT modifiers
- aggregate functions, window functions, scalar functions, casts, `CASE`,
  variables, parameter markers, regular expressions, row constructors,
  subqueries, quantified comparisons, JSON operators, and assignment operators
- full collation coercibility, non-ASCII pattern equivalence, temporal
  comparison conversions, SQL-mode-sensitive expression parsing, and complete
  expression metadata
- optimizer rewrites, index range scans, predicate pushdown to SQLite, and
  constant folding unless each transformation is proven not to alter MySQL
  result, warning, or error behavior

Task 17 is a statement feature, not new operator work. It must not mark
`UPDATE`, `DELETE`, joins, `HAVING`, `ON`, full projection expressions, or
optimizer behavior as supported.

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, Problems with Column Aliases:
  https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, Logical Operators:
  https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html
- MySQL 8.4 Reference Manual, String Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS` statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/schema-lifecycle/specs.md`
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
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
DROP DATABASE IF EXISTS mylite_task17_where;
CREATE DATABASE mylite_task17_where;
USE mylite_task17_where;

CREATE TABLE t (
  id INT PRIMARY KEY,
  n INT,
  s VARCHAR(20),
  z VARCHAR(20),
  nullable INT NULL,
  CamelCase INT
);

INSERT INTO t VALUES
  (1, 0, 'alpha', '2', NULL, 10),
  (2, 1, 'beta', '2a', 5, 20),
  (3, 2, 'ALPHA', 'a', NULL, 30),
  (4, NULL, 'gamma', '10', 0, 40);
```

The result order without `ORDER BY` is not a semantic guarantee. The examples
below list MySQL's observed simple-table order for readability; implementation
tests should compare row sets until Task 18 adds ordering support.

### Predicate truth and row visibility

The `WHERE` expression is evaluated as a row predicate. A row is selected only
when the predicate result is true. False and `NULL` / unknown predicate results
filter the row out.

Representative runtime results:

| SQL | Result row ids | Warnings |
| --- | --- | --- |
| `SELECT id FROM t WHERE 1` | `1`, `2`, `3`, `4` | none |
| `SELECT id FROM t WHERE 0` | empty | none |
| `SELECT id FROM t WHERE NULL` | empty | none |
| `SELECT id FROM t WHERE 'abc'` | empty | one 1292 warning for truncated numeric conversion |
| `SELECT id FROM t WHERE n` | `2`, `3` | none |
| `SELECT id FROM t WHERE nullable` | `2` | none |
| `SELECT id FROM t WHERE nullable IS NULL` | `1`, `3` | none |
| `SELECT id FROM t WHERE nullable <=> NULL` | `1`, `3` | none |

String predicates use MySQL truthiness conversion. The constant predicate
`'abc'` converts to numeric zero and records one warning, so no rows are
selected.

### Name resolution and aliases

`WHERE` resolves column references against tables in the `FROM` clause, not
against the projection output. Projection aliases are not visible in `WHERE`.

Representative runtime results and diagnostics:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT id FROM t WHERE t.n = 1` | returns row id `2` |
| `SELECT id FROM t WHERE mylite_task17_where.t.n = 2` | returns row id `3` |
| `SELECT id FROM t AS tt WHERE tt.n = 1` | returns row id `2` |
| `SELECT id FROM t AS tt WHERE t.n = 1` | error 1054 / `42S22`, unknown column `t.n` in `where clause` |
| `SELECT n AS x FROM t WHERE x = 1` | error 1054 / `42S22`, unknown column `x` in `where clause` |
| `SELECT id FROM t WHERE missing_col = 1` | error 1054 / `42S22`, unknown column `missing_col` in `where clause` |
| `SELECT id FROM t WHERE missing_alias.n = 1` | error 1054 / `42S22`, unknown column `missing_alias.n` in `where clause` |
| `SELECT id FROM t WHERE camelcase = 20` | returns row id `2` |

When a table alias is present, that alias is the visible table qualifier for
predicate column references. The base table name and schema-qualified base name
are hidden for predicate resolution, matching Task 15 projection behavior.

On the verified Linux MySQL runtime, table and alias qualifiers are
case-sensitive while column-name lookup is case-insensitive. This follows the
existing MyLite direction: byte-preserving, case-sensitive schema/table/alias
matching and case-insensitive column matching.

Ambiguous unqualified predicate columns require a multi-table query. MySQL
reports 1052 / SQLSTATE `23000`, for example:

```sql
SELECT t.id, u.id FROM t JOIN t AS u WHERE id = 1;
```

The message is `Column 'id' in where clause is ambiguous`. Task 17 has only one
base table, so ambiguous-column implementation tests should remain deferred to
join tasks while recording this MySQL target behavior.

### Three-valued logic and supported operators

Task 17 consumes the Task 16 expression semantics. The predicate evaluator must
preserve MySQL's `TRUE`, `FALSE`, and `NULL` outcomes, then apply row filtering
only after the final predicate value is known.

Representative runtime results:

| SQL | Result row ids | Warnings |
| --- | --- | --- |
| `SELECT id FROM t WHERE n BETWEEN 1 AND 2` | `2`, `3` | none |
| `SELECT id FROM t WHERE n NOT BETWEEN 3 AND NULL` | `1`, `2`, `3` | none |
| `SELECT id FROM t WHERE s LIKE 'alpha'` | `1`, `3` | none |
| `SELECT 'a_c' LIKE 'a\\_c'` | `1` | none |
| `SELECT id FROM t WHERE nullable NOT LIKE '5'` | `4` | none |
| `SELECT id FROM t WHERE n IN (1, 2, NULL)` | `2`, `3` | none |
| `SELECT id FROM t WHERE n NOT IN (1, 2, NULL)` | empty | none |

`LIKE` follows the current default case-insensitive collation for ASCII
examples. `NULL NOT LIKE pattern` still returns `NULL`, which filters the row
out. `IN` returns `NULL` when no match is found and at least one list element is
`NULL`, so `NOT IN` over such a list can also filter out otherwise nonmatching
rows.

### Conversion warnings and predicate evaluation order

Conversion warnings are statement-visible even when the row is filtered out.
The warning count is based on expressions MySQL actually evaluates, not on
selected rows.

Representative runtime results:

| SQL | Result row ids | Warnings |
| --- | --- | --- |
| `SELECT id FROM t WHERE z = 2` | `1`, `2` | two 1292 warnings for `2a` and `a` |
| `SELECT id FROM t WHERE z < 2` | `3` | two 1292 warnings for `2a` and `a` |
| `SELECT id FROM t WHERE 1 / 0` | empty | one 1365 warning, `Division by 0` |
| `SELECT id FROM t WHERE n = 1 AND z = 2` | `2` | one 1292 warning for `2a` |
| `SELECT id FROM t WHERE z = 2 OR n = 1` | `1`, `2` | two 1292 warnings for `2a` and `a` |
| `SELECT id FROM t WHERE n = 1 OR z = 2` | `1`, `2` | one 1292 warning for `a` |
| `SELECT id FROM t WHERE 0 AND z = 2` | empty | none |
| `SELECT id FROM t WHERE 1 OR z = 2` | `1`, `2`, `3`, `4` | none |
| `SELECT id FROM t WHERE z = 2 OR 1` | `1`, `2`, `3`, `4` | none |

The `AND` and `OR` examples show why Task 17 must not delegate predicate
semantics to SQLite or reorder predicates freely. MyLite may introduce
constant simplifications only when tests prove MySQL-visible warnings and
errors remain unchanged.

### Predicate errors

Predicate errors are recorded in the session diagnostics area and stop the
statement:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT id FROM t WHERE s LIKE 'a%' ESCAPE 'xx'` | error 1210 / `HY000`, `Incorrect arguments to ESCAPE` |
| `SELECT id FROM t WHERE n IN ()` | syntax error 1064 / `42000` |
| `SELECT id FROM t WHERE missing_col = 1` | error 1054 / `42S22`, unknown column in `where clause` |
| `SELECT id FROM t WHERE missing_alias.n = 1` | error 1054 / `42S22`, unknown column in `where clause` |
| `SELECT t.id, u.id FROM t JOIN t AS u WHERE id = 1` | error 1052 / `23000`, ambiguous column in `where clause`; join support is deferred |

Division by zero in a `SELECT` predicate under the verified SQL mode returns
`NULL`, records warning 1365, and filters out all rows for `WHERE 1 / 0`; it is
not a statement error.

### Warning lifecycle

`SHOW COUNT(*) WARNINGS` and `SHOW WARNINGS` are diagnostic statements and do
not clear the warning list. A later nondiagnostic statement clears the previous
statement's warnings.

Verified sequence:

```sql
SELECT id FROM t WHERE z = 2;
SHOW COUNT(*) WARNINGS;
SHOW WARNINGS;
SHOW COUNT(*) WARNINGS;
SELECT 1;
SHOW COUNT(*) WARNINGS;
```

MySQL reports two warnings after the predicate statement, still reports two
after `SHOW WARNINGS`, and reports zero after the later `SELECT 1`.

For predicate errors, `SHOW WARNINGS` reports the error condition. For example,
after `SELECT id FROM t WHERE missing_col = 1`, `SHOW COUNT(*) WARNINGS`
returns `1` and `SHOW WARNINGS` reports error 1054.

### Result metadata

`WHERE` does not add output columns and does not change projection metadata.
With `mysql --column-type-info -vvv`, this query:

```sql
SELECT n AS x, s FROM t WHERE z = 2;
```

returns two output columns. The first has client-visible field name `x`, origin
table `t`, and type metadata from `n`; the second has field name `s`, origin
table `t`, and type metadata from `s`. The predicate-only column `z` is not
exposed in result metadata.

## MyLite behavior

### Parser and AST

The parser should extend the current `SELECT` statement shape with an optional
WHERE clause after the table reference. The existing child order should remain
stable:

1. `MYLITE_SQL_AST_SELECT_LIST`
2. `MYLITE_SQL_AST_FROM_TABLE` or `MYLITE_SQL_AST_FROM_DUAL`
3. optional `MYLITE_SQL_AST_WHERE_CLAUSE`

Recommended AST addition:

- `MYLITE_SQL_AST_WHERE_CLAUSE`, with the predicate expression as its only
  child and a source span covering `WHERE expression`

The expression tree itself should reuse Task 16 nodes. Do not add
predicate-specific operator nodes.

### Lemon grammar snippets

These snippets describe MyLite's intended Task 17 grammar shape. They are not
copied from MySQL grammar.

```lemon
select_statement ::= SELECT select_item_list FROM table_name opt_table_alias opt_where_clause.
select_statement ::= SELECT STAR FROM table_name opt_table_alias opt_where_clause.

opt_where_clause ::= .
opt_where_clause ::= where_clause.

where_clause ::= WHERE expression.
```

The clause must be accepted after `FROM table_name [alias]` and before future
`GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, and locking clauses. Bare table
aliases must not consume `WHERE` as an alias token.

These MySQL-supported forms remain outside Task 17 unless the implementation
can add them without broadening the feature scope:

```lemon
/* Deferred: rowless SELECT predicates. */
select_statement ::= SELECT select_item_list where_clause.
select_statement ::= SELECT select_item_list FROM DUAL where_clause.

/* Deferred: joins and predicate placement across joins. */
select_statement ::= SELECT select_item_list FROM table_reference_list where_clause.
join_condition ::= ON expression.

/* Deferred: grouped and post-group predicates. */
select_statement ::= SELECT select_item_list FROM table_name opt_table_alias where_clause GROUP BY group_list.
select_statement ::= SELECT select_item_list FROM table_name opt_table_alias HAVING expression.

/* Deferred: mutation predicates. */
update_statement ::= UPDATE table_name SET assignment_list where_clause.
delete_statement ::= DELETE FROM table_name where_clause.
```

### Name-resolution context

Task 17 should add a row expression binding path instead of evaluating
predicate identifiers as no-table expressions. The binder should receive:

- the resolved table schema and base table name
- the optional table alias
- catalog-ordered column metadata from `__mylite_column_catalog`
- a diagnostic context string, `where clause`

Resolution rules:

- Unqualified column references match table columns case-insensitively.
- Two-part references match the visible table identity and then the column.
  The visible table identity is the alias when present, otherwise the base
  table name.
- Three-part references match `schema.table.column` only when no table alias is
  present and the schema and table match the resolved table identity.
- Schema, table, and alias qualifier comparisons use the same case-sensitive
  policy as Task 15 table-backed SELECT.
- Projection aliases are never visible to the predicate binder.
- Unknown predicate columns and unknown predicate qualifiers use the
  1054-style unknown-column diagnostic with `in 'where clause'`.
- Ambiguous unqualified column diagnostics are not reachable in the one-table
  scope, but the binder should be designed so joins can report the verified
  1052-style diagnostic later.

The binder should reject unsupported expression shapes with a deterministic
unsupported-expression diagnostic unless MySQL itself reports a syntax or
semantic error for the accepted shape.

### Runtime execution

The first implementation should prefer a MyLite-owned predicate evaluator over
SQLite `WHERE` translation. SQLite's operators do not match MySQL for
truthiness, `<=>`, string-to-number warnings, `LIKE`, unsigned bit operations,
division warnings, and warning timing.

Recommended execution model:

1. Build the Task 15 single-table select plan.
2. Bind the optional predicate against the same resolved table context.
3. Determine the union of physical columns needed by projection and predicate.
4. Read physical rows from SQLite without applying the MySQL predicate in
   SQLite.
5. For each physical row, materialize the row values needed by the predicate in
   a row evaluation context.
6. Evaluate the predicate with Task 16 semantics plus row column lookup.
7. Keep the row only when the final predicate truth value is true.
8. Produce projection columns and Task 15 result metadata for kept rows.

This custom path may scan more columns than the projection needs. It is
acceptable for Task 17 because correctness, warning behavior, and diagnostics
are the priority. Later optimizer work may lower proven-safe predicates or
subexpressions to SQLite.

Predicate evaluation details:

- A `NULL` predicate result filters the row out.
- Integer zero and decimal/real zero are false; nonzero numeric values are
  true.
- String truthiness uses Task 16 numeric conversion and warning behavior.
- Warning records append in MySQL-observed evaluation order for the supported
  expression subset.
- `AND` and `OR` must short-circuit in ways that preserve MySQL-visible
  warnings and errors for supported cases.
- Statement warnings are cleared when the predicate statement begins and remain
  available after success or failure until a later nondiagnostic statement
  clears them.
- The statement is read-only and must not alter affected rows, last insert id,
  catalog rows, or physical table contents.

### Diagnostics and warnings

MyLite's diagnostics must ultimately expose MySQL numeric error codes,
SQLSTATEs, messages, and warning rows. If the current public API cannot expose
all of them when Task 17 is implemented, the implementation should still store
the structured warning/error records internally and document any temporary
exposure gap in `COMPATIBILITY.md`.

Required target diagnostics for Task 17:

- 1054 / `42S22`: `Unknown column 'name' in 'where clause'`
- 1052 / `23000`: `Column 'name' in where clause is ambiguous`, deferred until
  joins make ambiguity reachable
- 1064 / `42000`: syntax errors such as an empty `IN ()` list
- 1210 / `HY000`: invalid `LIKE ... ESCAPE` arguments
- 1292 / `22007`: truncated numeric conversion warnings
- 1365 / `22012`: division-by-zero warnings in SELECT predicates

Current MyLite message-only errors should still use the MySQL wording where
the project already has no numeric-code surface.

### Result metadata model

Task 17 does not require new public result metadata. Projection metadata should
remain governed by Task 15 and Task 23:

- `mylite_column_name()` returns the projection label.
- Schema, visible table, origin table, and origin column metadata come from the
  projected output column.
- Predicate-only columns do not appear in result metadata.
- Computed projection expression metadata remains deferred.

### Storage and performance implications

Task 17 does not change the `.mylite` file format, persistent table catalog, or
schema catalog. It adds statement-runtime state:

- a bound predicate expression
- a row value context for predicate column references
- warning records produced during predicate evaluation
- scratch storage for conversions and temporary expression values

The evaluator should avoid per-row heap churn for common integer, NULL, and
short string values. If string or decimal conversions allocate, the storage
should be statement-owned or scratch-owned with clear lifetime rules.

The initial implementation may perform a full table scan. That is compatible
for Task 17 as long as row visibility, warnings, errors, and metadata match
MySQL. Index selection, predicate pushdown, and constant folding are future
optimizer features.

## Explicit deferred behavior

- `WHERE` on no-table scalar SELECT is deferred. The covered scalar
  `FROM DUAL WHERE` subset is implemented for standalone DUAL selects and
  `INSERT ... SELECT FROM DUAL`.
- Joins and ambiguous predicate references are deferred.
- `ON` and `HAVING` predicates are deferred.
- `WHERE` for `UPDATE` and `DELETE` is deferred to their statement tasks.
- `SHOW ... WHERE` filtering is handled by the shared SHOW filter for supported
  SHOW metadata statements; broader SHOW predicate expressions remain deferred.
- Predicate subqueries, `EXISTS`, row constructors, quantified comparisons,
  variables, parameters, functions, casts, collations, regular expressions,
  JSON operators, and aggregate functions are deferred.
- Full temporal comparison conversion, non-ASCII collation behavior,
  `NO_BACKSLASH_ESCAPES`, `PIPES_AS_CONCAT`, `HIGH_NOT_PRECEDENCE`, and other
  SQL-mode-sensitive expression behavior are deferred unless already supported
  by the expression foundation.
- Optimizer behavior, index usage, constant folding, and SQLite predicate
  pushdown are deferred unless each case is MySQL-runtime verified.

## MySQL-runtime-verified test expectations

Implementation tests should compare MyLite against MySQL 8.4.9 for at least
these cases. Unless a test introduces `ORDER BY` after Task 18, compare result
sets rather than relying on row order.

### Parser tests

| SQL | Expected parser outcome |
| --- | --- |
| `SELECT id FROM t WHERE 1` | accepted with a `MYLITE_SQL_AST_WHERE_CLAUSE` child |
| `SELECT * FROM t WHERE n` | accepted |
| `SELECT t.* FROM t WHERE t.n = 1` | accepted |
| `SELECT id FROM t AS tt WHERE tt.n = 1` | accepted |
| `SELECT id FROM t tt WHERE tt.n = 1` | accepted |
| `SELECT id FROM t WHERE n BETWEEN 1 AND 2` | accepted |
| `SELECT id FROM t WHERE s LIKE 'a%' ESCAPE '!'` | accepted |
| `SELECT id FROM t WHERE n IN (1, 2, NULL)` | accepted |
| `SELECT id FROM t WHERE n IN ()` | syntax error 1064 / `42000` |
| `SELECT id FROM t WHERE n = 1 ORDER BY id` | remains unsupported until Task 18 |
| `SELECT id FROM t JOIN t AS u WHERE t.id = u.id` | remains unsupported until joins |

### Predicate truth and NULL filtering

| SQL | Expected result row ids | Expected warnings |
| --- | --- | --- |
| `SELECT id FROM t WHERE 1` | `1`, `2`, `3`, `4` | none |
| `SELECT id FROM t WHERE 0` | empty | none |
| `SELECT id FROM t WHERE NULL` | empty | none |
| `SELECT id FROM t WHERE 'abc'` | empty | one 1292 warning |
| `SELECT id FROM t WHERE n` | `2`, `3` | none |
| `SELECT id FROM t WHERE nullable` | `2` | none |
| `SELECT id FROM t WHERE nullable IS NULL` | `1`, `3` | none |
| `SELECT id FROM t WHERE nullable <=> NULL` | `1`, `3` | none |

### Name resolution

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `SELECT id FROM t WHERE t.n = 1` | returns row id `2` |
| `SELECT id FROM t WHERE mylite_task17_where.t.n = 2` | returns row id `3` |
| `SELECT id FROM t AS tt WHERE tt.n = 1` | returns row id `2` |
| `SELECT id FROM t AS tt WHERE t.n = 1` | unknown column `t.n` in `where clause` |
| `SELECT n AS x FROM t WHERE x = 1` | unknown column `x` in `where clause`; projection alias is not visible |
| `SELECT id FROM t WHERE missing_col = 1` | unknown column `missing_col` in `where clause` |
| `SELECT id FROM t WHERE missing_alias.n = 1` | unknown column `missing_alias.n` in `where clause` |
| `SELECT id FROM t WHERE camelcase = 20` | returns row id `2`; column lookup is case-insensitive |
| `SELECT t.id, u.id FROM t JOIN t AS u WHERE id = 1` | MySQL reports ambiguous column 1052; MyLite defers until joins |

### Operators and three-valued logic

| SQL | Expected result row ids | Expected warnings |
| --- | --- | --- |
| `SELECT id FROM t WHERE n BETWEEN 1 AND 2` | `2`, `3` | none |
| `SELECT id FROM t WHERE n NOT BETWEEN 3 AND NULL` | `1`, `2`, `3` | none |
| `SELECT id FROM t WHERE s LIKE 'alpha'` | `1`, `3` | none |
| `SELECT 'a_c' LIKE 'a\\_c'` | scalar result `1` | none |
| `SELECT id FROM t WHERE nullable NOT LIKE '5'` | `4` | none |
| `SELECT id FROM t WHERE n IN (1, 2, NULL)` | `2`, `3` | none |
| `SELECT id FROM t WHERE n NOT IN (1, 2, NULL)` | empty | none |

### Conversion warnings and short-circuit behavior

| SQL | Expected result row ids | Expected warnings |
| --- | --- | --- |
| `SELECT id FROM t WHERE z = 2` | `1`, `2` | two 1292 warnings for `2a` and `a` |
| `SELECT id FROM t WHERE z < 2` | `3` | two 1292 warnings for `2a` and `a` |
| `SELECT id FROM t WHERE 1 / 0` | empty | one 1365 warning |
| `SELECT id FROM t WHERE n = 1 AND z = 2` | `2` | one 1292 warning for `2a` |
| `SELECT id FROM t WHERE z = 2 OR n = 1` | `1`, `2` | two 1292 warnings for `2a` and `a` |
| `SELECT id FROM t WHERE n = 1 OR z = 2` | `1`, `2` | one 1292 warning for `a` |
| `SELECT id FROM t WHERE 0 AND z = 2` | empty | none |
| `SELECT id FROM t WHERE 1 OR z = 2` | `1`, `2`, `3`, `4` | none |
| `SELECT id FROM t WHERE z = 2 OR 1` | `1`, `2`, `3`, `4` | none |

### Predicate errors and diagnostics

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `SELECT id FROM t WHERE s LIKE 'a%' ESCAPE 'xx'` | error 1210 / `HY000`, incorrect `ESCAPE` arguments |
| `SELECT id FROM t WHERE n IN ()` | syntax error 1064 / `42000` |
| `SELECT id FROM t WHERE missing_col = 1` | error 1054 / `42S22`; condition appears in warning list |
| `SELECT id FROM t WHERE missing_alias.n = 1` | error 1054 / `42S22`; condition appears in warning list |
| `SELECT t.id, u.id FROM t JOIN t AS u WHERE id = 1` | error 1052 / `23000`, deferred until joins |

### Warning lifecycle

| SQL sequence | Expected behavior |
| --- | --- |
| `SELECT id FROM t WHERE z = 2; SHOW COUNT(*) WARNINGS; SHOW WARNINGS;` | warning count `2`, with two 1292 warning rows |
| `SHOW COUNT(*) WARNINGS` immediately after `SHOW WARNINGS` | still returns `2` |
| `SELECT 1; SHOW COUNT(*) WARNINGS;` after the warning-producing statement | returns `0` because `SELECT 1` clears the previous warning list |
| failed predicate statement, then `SHOW WARNINGS` | shows the error condition recorded for that statement |

### Metadata and side effects

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `SELECT n AS x, s FROM t WHERE z = 2` | output columns are `x` and `s`; predicate column `z` is not exposed |
| table-backed SELECT with `WHERE` | result schema/table/origin metadata remains governed by projection columns |
| table-backed SELECT with `WHERE` | read-only; no affected rows, last insert id, catalog mutation, or physical table mutation |

## Implementation handoff notes

- Add `MYLITE_SQL_AST_WHERE_CLAUSE` and parser helpers before changing runtime
  planning.
- Extend the table SELECT plan with an optional bound predicate and a row
  expression context.
- Reuse Task 15 table and column metadata loading for predicate binding.
- Extend Task 16 expression evaluation so identifiers can read from the current
  row; keep no-table expression evaluation intact.
- Avoid SQLite `WHERE` lowering for the initial implementation. Scan rows and
  evaluate predicates in MyLite until a later optimizer can prove safe
  pushdown.
- Preserve warning-list behavior before adding optimizer shortcuts. The
  short-circuit tests above should be part of the first runtime test batch.
