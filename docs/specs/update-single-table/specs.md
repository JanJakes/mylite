# Single-table UPDATE

## Scope

This feature specifies Task 19, executable single-table `UPDATE` for user base
tables created by MyLite's supported `CREATE TABLE` subset. It is the first
mutation statement that reuses the Task 15 table scan, Task 16 expression
evaluator, Task 17 predicate binder, Task 18 ordering/windowing design, and
Task 13/14 write metadata.

In scope:

- `UPDATE table_name [alias] SET assignment_list`
- schema-qualified targets and selected-schema target resolution
- target aliases using `AS alias` or a bare identifier alias
- unqualified, table-qualified, schema-table-qualified, and alias-qualified
  assignment targets where MySQL accepts them
- assignment values using the Task 16 expression subset or the `DEFAULT`
  keyword
- uncorrelated scalar subquery assignment values
- source-order assignment evaluation, including repeated assignment targets
- row column references in assignment expressions, with earlier assignments
  visible to later assignments for the same row
- `WHERE` filtering over the Task 17 predicate subset
- `ORDER BY` over the Task 18 order-expression subset
- `LIMIT row_count` as a rows-matched restriction
- no-op updates, affected rows, matched rows, warning counts, and row-count
  diagnostics
- primary-key and unique-key updates, including order-sensitive conflict
  avoidance and statement rollback on conflicts
- explicit defaults, nullable implicit defaults, required-column diagnostics,
  and default-expression hooks for columns whose defaults are already modeled
- `AUTO_INCREMENT` updates, next-sequence effects, and session last insert id
  side effects
- nullability, default, duplicate-key, unsupported-expression, and rollback
  diagnostics for the currently supported value types
- statement warning and diagnostic lifecycle hooks
- storage, catalog, and transaction effects for single-table updates

Out of scope for Task 19:

- multiple-table `UPDATE` and joined table references
- common table expressions before `UPDATE`
- `LOW_PRIORITY`, joined `UPDATE IGNORE`, and conversion-error demotion caused
  specifically by `IGNORE`
- partition clauses, optimizer hints, and locking modifiers; index hints are
  parsed and ignored by the separate placeholder slice
- `ORDER BY` or `LIMIT` variants other than single `LIMIT row_count`
- correlated subqueries, `EXISTS`, quantified comparisons, row constructors,
  parameters, and other expression shapes outside the current shared scalar
  expression evaluator
- `DEFAULT(col_name)` function syntax
- generated-column runtime support until generated columns exist in MyLite's
  catalog and write path
- triggers, foreign keys, cascading actions, check constraints, views,
  privileges, binary logging, replication safety, and optimizer plan details
- full strict/non-strict assignment and predicate conversion behavior beyond
  the existing expression/value foundations
- full SQL-mode management beyond the verified default strict mode and the
  documented non-strict warning baseline

Task 19 is implemented for the executable single-table subset described above.
The remaining out-of-scope forms stay deferred and are tracked in the
compatibility matrix.

## Sources

- MySQL 8.4 Reference Manual, `UPDATE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/update.html
- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, Generated Columns:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-generated-columns.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/column-attributes/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`
  under the default strict SQL mode unless a non-strict probe is explicitly
  noted.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Test fixtures used for runtime verification

Most probes used this base shape:

```sql
DROP DATABASE IF EXISTS mylite_task19_update;
CREATE DATABASE mylite_task19_update;
USE mylite_task19_update;

CREATE TABLE t (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  a INT DEFAULT 3,
  b INT DEFAULT 4,
  c INT NULL,
  s VARCHAR(20),
  z VARCHAR(20),
  u INT UNIQUE,
  nn INT NOT NULL DEFAULT 7,
  must INT NOT NULL,
  CamelCase INT,
  KEY idx_a (a)
) AUTO_INCREMENT=10;

INSERT INTO t (a,b,c,s,z,u,nn,must,CamelCase) VALUES
  (1,10,NULL,'alpha','2',1,7,100,11),
  (2,20,5,'beta','2a',2,7,200,22),
  (3,30,NULL,'gamma','a',3,7,300,33),
  (4,40,0,'delta','10',4,7,400,44);
```

Additional focused fixtures covered primary-key conflicts, non-strict warnings,
generated columns, and `AUTO_INCREMENT` side effects.

### Syntax and target resolution

Single-table `UPDATE` updates rows in one target table. The `SET` list is
mandatory and contains one or more assignments. With no `WHERE` clause, all
target rows are candidates.

Target resolution matches existing DML target behavior:

| SQL or condition | MySQL behavior |
| --- | --- |
| no selected schema for unqualified `UPDATE t ...` | error 1046 / `3D000`, `No database selected` |
| `UPDATE missing_schema.t ...` | error 1049 / `42000`, unknown database |
| `UPDATE existing_schema.missing_t ...` | error 1146 / `42S02`, table does not exist |
| `UPDATE missing_t ...` in selected existing schema | error 1146 / `42S02`, table does not exist |

MySQL accepts target aliases using `AS` or a bare identifier:

```sql
UPDATE t AS tt SET tt.a = tt.a + 1 WHERE tt.id = 12;
UPDATE t tt SET tt.a = tt.a + 1 WHERE tt.id = 12;
```

When an alias is present, the alias is the visible table qualifier. Base table
and schema-qualified base-table qualifiers are hidden:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE t AS tt SET tt.a = tt.a + 1 WHERE tt.id = 12` | succeeds |
| `UPDATE t AS tt SET t.a = 1 WHERE tt.id = 12` | error 1054 / `42S22`, unknown column `t.a` in `field list` |
| `UPDATE t AS tt SET tt.a = 1 WHERE t.id = 12` | error 1054 / `42S22`, unknown column `t.id` in `where clause` |
| `UPDATE t AS tt SET tt.a = 1 ORDER BY t.id LIMIT 1` | error 1054 / `42S22`, unknown column `t.id` in `order clause` |
| `UPDATE schema.t AS tt SET schema.t.a = 1 WHERE tt.id = 1` | error 1054 / `42S22`, unknown column `schema.t.a` in `field list` |

Without an alias, assignment targets and expression references may be
unqualified, table-qualified, or schema-table-qualified:

```sql
UPDATE mylite_task19_update.t
SET t.a = t.a + 1, mylite_task19_update.t.b = t.a
WHERE id = 1;
```

Column-name lookup is case-insensitive for the verified identifier repertoire.
Schema, table, and alias qualifier matching follows the existing MyLite policy
from table-backed `SELECT`: byte-preserving and case-sensitive on the verified
Linux MySQL runtime.

### Assignment semantics

Assignment targets identify columns in the target table. The right-hand side
may be an expression or `DEFAULT`. MySQL evaluates single-table assignments in
source order. Column references see the row's current candidate values, so
earlier assignments are visible to later assignments for the same row.
Uncorrelated scalar subquery assignment values follow the existing scalar
subquery contract: the subquery must return one column, a one-row result assigns
that value, an empty result assigns `NULL`, and more than one row is an
execution error.

Representative runtime results:

| SQL | Stored effect |
| --- | --- |
| `UPDATE t SET a = a + 1, b = a WHERE id = 10` | row `10` becomes `a=2`, `b=2` |
| `UPDATE t SET a = 100, a = a + 1 WHERE id = 11` | duplicate target is accepted; row `11` becomes `a=101` |
| `UPDATE t SET a = a WHERE id IN (10,11)` | matches two rows, changes zero rows |
| `UPDATE t SET s = (SELECT label FROM lookup WHERE id = 1) WHERE id = 10` | assigns the scalar subquery value |

Repeated assignment targets are not an error for `UPDATE`. This differs from
`INSERT ... SET`, where duplicate target columns are rejected.

Unknown assignment targets and unknown right-hand-side column references use
the `field list` diagnostic context:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE t SET missing_col = 1 WHERE id = 1` | error 1054 / `42S22`, unknown column in `field list` |
| `UPDATE t SET a = missing_col WHERE id = 1` | error 1054 / `42S22`, unknown column in `field list` |
| `UPDATE t SET missing_col = 1, a = 2 WHERE id = 1` | reports the unknown assignment target before mutation |

### Defaults, generated columns, and `AUTO_INCREMENT`

`DEFAULT` assigns the target column's default value.

Representative runtime results:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE t SET a = DEFAULT, c = DEFAULT, nn = DEFAULT WHERE id = 13` | stores `a=3`, `c=NULL`, `nn=7` |
| `UPDATE t SET must = DEFAULT WHERE id = 13` | error 1364 / `HY000`, required column has no default |
| `UPDATE ai SET id = DEFAULT WHERE v = 1` for `AUTO_INCREMENT` primary key | stores `id=0`; it does not generate a new id |
| `UPDATE ai SET id = NULL WHERE v = 40` | error 1048 / `23000`, column cannot be null |
| `UPDATE ai SET id = 100 WHERE id = 6`; then `INSERT INTO ai (v) VALUES (40)` | next generated id is `101` |
| `UPDATE ai SET id = 0 WHERE id = 5` | stores zero; last insert id is unchanged |

Updating an existing `AUTO_INCREMENT` column to a nonzero high value resets the
next generated value. Ordinary updates and explicit `AUTO_INCREMENT` updates do
not change the session `LAST_INSERT_ID()`.

MySQL also supports `LAST_INSERT_ID(expr)` inside an update expression:

```sql
CREATE TABLE seq (id INT NOT NULL);
INSERT INTO seq VALUES (0);
UPDATE seq SET id = LAST_INSERT_ID(id + 1);
SELECT LAST_INSERT_ID();
```

The update changes `seq.id` to `1` and makes `LAST_INSERT_ID()` return `1`.
MyLite should preserve this side-effect contract once information functions are
implemented. Task 19 should reject function calls deterministically if Task 24
has not provided them yet.

Generated-column runtime behavior was verified with:

```sql
CREATE TABLE g (
  id INT PRIMARY KEY,
  a INT,
  b INT GENERATED ALWAYS AS (a + 1) STORED,
  c INT GENERATED ALWAYS AS (a + 2) VIRTUAL
);
INSERT INTO g (id,a) VALUES (1,10),(2,20);
```

Observed behavior:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE g SET a = a + 5 WHERE id = 1` | generated values are recomputed; row `1` reads as `a=15`, `b=16`, `c=17` |
| `UPDATE g SET b = DEFAULT, c = DEFAULT WHERE id = 1` | accepted; matches one row, changes zero rows |
| `UPDATE g SET b = 99 WHERE id = 1` | error 3105 / `HY000`, generated column value is not allowed |

MyLite generated-column DDL and runtime support are not present yet, so Task 19
must include design hooks and explicit deferred diagnostics instead of silently
pretending generated columns are ordinary stored columns.

### WHERE interaction

The optional `WHERE` clause filters candidate rows before assignment
evaluation. Only rows whose predicate is true are matched for update. False and
`NULL` predicates do not match.

Name resolution in `WHERE` follows Task 17 rules with an update-specific target
table context:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE t SET a = 1 WHERE id = 10` | row `10` is the only matched row |
| `UPDATE t AS tt SET tt.a = 1 WHERE tt.id = 10` | alias-qualified predicate succeeds |
| `UPDATE t AS tt SET tt.a = 1 WHERE t.id = 10` | error 1054 / `42S22`, unknown column in `where clause` |
| `UPDATE t SET a = 1 WHERE missing_col = 1` | error 1054 / `42S22`, unknown column in `where clause` |

Predicate conversion behavior differs from `SELECT` under the default strict
SQL mode because `UPDATE` is a data-change statement. For example:

```sql
CREATE TABLE w (id INT PRIMARY KEY, a INT, z VARCHAR(20), nn INT NOT NULL);
INSERT INTO w VALUES (1,0,'2',1),(2,0,'2a',2),(3,0,'a',3),(4,0,'10',4);
UPDATE w SET a = a + 1 WHERE z = 2;
```

The default strict-mode update fails with error 1292 /
`Truncated incorrect DOUBLE value: '2a'` and rolls back all row changes.

With `SET SESSION sql_mode = ''`, the same predicate succeeds, matches rows
`1` and `2`, changes both, and records two 1292 warnings for `'2a'` and `'a'`.
This non-strict behavior is a future SQL-mode target if Task 19 lands before
full SQL-mode support.

### ORDER BY and LIMIT interaction

For single-table `UPDATE`, `ORDER BY` determines the order in which matching
rows are updated. `LIMIT row_count` restricts the number of rows matched, not
the number of rows actually changed. There is no offset form for `UPDATE`
`LIMIT`.

Representative runtime results:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE pk SET id = id + 1` for primary keys `1,2,3,4` | duplicate-key error 1062; table unchanged |
| `UPDATE pk SET id = id + 1 ORDER BY id DESC` | succeeds by updating larger keys first |
| `UPDATE pk SET v = v + 1 ORDER BY v DESC LIMIT 2` | updates the two largest `v` values |
| `UPDATE pk SET note = note ORDER BY v ASC LIMIT 2` | matches two rows, changes zero rows |
| `UPDATE pk SET note = 'x' WHERE v >= 5 ORDER BY v DESC LIMIT 1` | filters, orders, then updates one row |

`LIMIT` syntax observations:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE t SET a = a + 1 LIMIT 1` | accepted |
| `UPDATE t SET a = a + 1 LIMIT 1 OFFSET 1` | syntax error 1064 / `42000` |
| `UPDATE t SET a = a + 1 LIMIT 1, 1` | syntax error 1064 / `42000` |
| `UPDATE t SET a = a + 1 LIMIT -1` | syntax error 1064 / `42000` |
| `UPDATE t SET a = a + 1 LIMIT 1.5` | syntax error 1064 / `42000` |
| `UPDATE t SET a = a + 1 LIMIT '1'` | syntax error 1064 / `42000` |
| `UPDATE t SET a = a + 1 LIMIT NULL` | syntax error 1064 / `42000` |
| `UPDATE t SET a = a + 1 LIMIT 18446744073709551615` | accepted |
| `UPDATE t SET a = a + 1 LIMIT 18446744073709551616` | syntax error 1064 / `42000` |

`ORDER BY` expressions are evaluated over pre-update row values and target
table columns. There are no projection aliases in `UPDATE`, so Task 18's
projection-alias lookup rules do not apply.

Under strict SQL mode, warning-producing order expressions can become update
errors. For example, `UPDATE w SET a = a + 1 ORDER BY z + 0, id LIMIT 2`
fails with 1292 when `z` contains `'2a'`.

### Affected rows, matched rows, and diagnostics

MySQL reports changed rows as the default affected-row value. The client info
string also reports matched rows and warnings:

```text
Rows matched: 2  Changed: 0  Warnings: 0
```

Representative runtime results:

| SQL | `ROW_COUNT()` / affected rows | Matched rows |
| --- | --- | --- |
| `UPDATE t SET a = a + 1 WHERE id = 10` | `1` | `1` |
| `UPDATE t SET a = 999 WHERE id = 999` | `0` | `0` |
| `UPDATE t SET a = a WHERE id IN (10,11)` | `0` | `2` |
| `UPDATE pk SET note = note ORDER BY v ASC LIMIT 2` | `0` | `2` |

The C API can request matched-row affected counts with `CLIENT_FOUND_ROWS`.
MyLite's first Task 19 implementation should expose MySQL's default changed
row count through `mylite_affected_rows()` and keep the statement internals
capable of reporting matched rows when client capability flags are added.

After a failed `UPDATE`, `ROW_COUNT()` returns `-1`. The statement warning list
contains the error condition until a later nondiagnostic statement clears it.
`SHOW COUNT(*) WARNINGS` and `SHOW WARNINGS` inspect the list without clearing
it. A `SELECT ROW_COUNT()` call is itself a nondiagnostic result-producing
statement, so tests that need warning rows should inspect warnings before
running unrelated `SELECT` statements.

### Conversion, NULL, and strict-mode behavior

Under the default strict SQL mode:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE w SET a = 'abc' WHERE id = 1` | error 1366 / `HY000`, incorrect integer value |
| `UPDATE w SET a = z + 0 WHERE id IN (2,3)` | error 1292 / `22007`, truncated incorrect double value |
| `UPDATE w SET a = 1 / 0 WHERE id = 1` | error 1365 / `22012`, division by zero |
| `UPDATE w SET a = a + 1 WHERE 1 / 0` | error 1365 / `22012`, division by zero |
| `UPDATE w SET nn = NULL WHERE id = 1` | error 1048 / `23000`, column cannot be null |

All of these strict-mode errors leave physical rows unchanged.

With an empty SQL mode, observed non-strict behavior includes:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE w SET a = a + 1 WHERE z = 2` | succeeds with two 1292 warnings and updates rows with `z='2'` and `z='2a'` |
| `UPDATE w SET a = z + 0 WHERE id IN (2,3)` | succeeds with two 1292 warnings; one row changes because `'a'` converts to `0` and was already `0` |
| `UPDATE w SET nn = NULL WHERE id = 1` | succeeds with warning 1048 and stores implicit default `0` |

Task 19 documents these MySQL behaviors but defers full conversion promotion
and non-strict warning demotion until the value/type and SQL-mode foundations
can support them consistently.

### Conflict handling and atomicity

Primary-key and unique-key conflicts abort the statement and leave the target
table unchanged. Verified examples:

```sql
CREATE TABLE pk (id INT PRIMARY KEY, v INT UNIQUE, note VARCHAR(10));
INSERT INTO pk VALUES (1,1,'one'),(2,2,'two'),(3,3,'three'),(4,4,'four');
UPDATE pk SET id = id + 1;
```

The update fails with duplicate-entry error 1062 for the primary key and the
table remains `1,2,3,4`.

Similarly, `UPDATE pk SET v = v + 1` fails with duplicate-entry error 1062 for
the unique key and leaves every row unchanged. Adding `ORDER BY v DESC` can
avoid the transient conflict because rows are updated in descending key order.

MyLite must preserve statement atomicity for validation, expression,
constraint, duplicate-key, binding, and SQLite failures.

## MyLite behavior

### Parser and AST

Add a single-table `update_statement` AST node that records:

1. target table name
2. optional target alias
3. assignment list in source order
4. optional `WHERE` clause
5. optional `ORDER BY` clause
6. optional `LIMIT` clause

Recommended AST additions:

- `MYLITE_SQL_AST_UPDATE_STATEMENT`
- `MYLITE_SQL_AST_UPDATE_TARGET`
- `MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST`
- `MYLITE_SQL_AST_UPDATE_ASSIGNMENT`
- `MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE`

Each assignment node stores the target qualified identifier and either an
expression child or a `DEFAULT` marker. Assignment source order is semantically
observable and must be preserved exactly.

### Lemon grammar snippets

These snippets describe MyLite's intended Task 19 grammar shape. They are not
copied from MySQL grammar.

```lemon
update_statement ::= UPDATE single_update_target SET update_assignment_list
    opt_where_clause opt_order_by_clause opt_update_limit_clause.

update_statement ::= UPDATE IGNORE single_update_target SET update_assignment_list
    opt_where_clause opt_order_by_clause opt_update_limit_clause.

single_update_target ::= table_name opt_table_alias.

opt_table_alias ::= .
opt_table_alias ::= table_alias.
opt_table_alias ::= AS table_alias.

table_alias ::= identifier.

update_assignment_list ::= update_assignment.
update_assignment_list ::= update_assignment_list COMMA update_assignment.

update_assignment ::= update_assignment_target EQ update_assignment_value.

update_assignment_target ::= qualified_identifier.

update_assignment_value ::= expression.
update_assignment_value ::= DEFAULT.

opt_where_clause ::= .
opt_where_clause ::= WHERE expression.

opt_order_by_clause ::= .
opt_order_by_clause ::= ORDER BY update_order_item_list.

update_order_item_list ::= update_order_item.
update_order_item_list ::= update_order_item_list COMMA update_order_item.

update_order_item ::= expression opt_order_direction.

opt_order_direction ::= .
opt_order_direction ::= ASC.
opt_order_direction ::= DESC.

opt_update_limit_clause ::= .
opt_update_limit_clause ::= LIMIT unsigned_integer_literal.
```

`SET`, `WHERE`, `ORDER`, and `LIMIT` are clause boundaries. Bare table aliases
must not consume those keywords as aliases.

The following MySQL-supported grammar surface remains outside Task 19:

```lemon
/* Deferred: modifiers. */
update_statement ::= UPDATE LOW_PRIORITY single_update_target SET update_assignment_list.

/* Deferred: common table expressions. */
update_statement ::= WITH cte_list UPDATE single_update_target SET update_assignment_list.

/* Deferred: partition selection. */
single_update_target ::= table_name PARTITION LPAREN identifier_list RPAREN
    opt_table_alias.

/* Deferred: joined and multiple-table updates. */
update_statement ::= UPDATE table_reference_list SET update_assignment_list
    opt_where_clause.
table_reference_list ::= table_reference_list COMMA table_reference.
table_reference ::= table_reference JOIN table_reference join_condition.

/* Deferred: unsupported UPDATE LIMIT forms. */
opt_update_limit_clause ::= LIMIT unsigned_integer_literal COMMA unsigned_integer_literal.
opt_update_limit_clause ::= LIMIT unsigned_integer_literal OFFSET unsigned_integer_literal.
```

### Binding and name resolution

The update binder should reuse the single-table resolution machinery from
Tasks 15 through 18 with an update-specific diagnostic context.

Target binding:

- Resolve `schema.table` against the written schema.
- Resolve unqualified `table` against the selected default schema.
- Reject missing selected schema, missing explicit schema, missing table, and
  system-schema targets before assignment binding.
- Load column metadata from `__mylite_column_catalog` in ordinal order.
- Load primary and unique index metadata needed for conflict checks.
- Load table-level `AUTO_INCREMENT` state from `__mylite_table_catalog`.

Visible table identity:

- If an alias exists, only the alias is a valid two-part qualifier.
- If no alias exists, the table name is a valid two-part qualifier.
- If no alias exists, `schema.table.column` is valid when schema and table
  match the resolved target.
- If an alias exists, base table and schema-qualified base table qualifiers are
  hidden for assignment, `WHERE`, and `ORDER BY` resolution.

Column lookup:

- Match column names case-insensitively.
- Match schema/table/alias qualifiers according to the existing
  byte-preserving case-sensitive schema/table policy.
- Assignment target duplicates are accepted and should not be collapsed.
- Unknown assignment targets and right-hand-side column references use the
  1054-style `field list` diagnostic.
- Unknown predicate references use the 1054-style `where clause` diagnostic.
- Unknown order references use the 1054-style `order clause` diagnostic.

The assignment expression binder should resolve column references against the
target table's candidate row context. The same expression tree can read
different values as assignments progress through the source list.

### Runtime execution model

The first implementation should use a MyLite-owned update executor rather than
delegating semantics to SQLite `UPDATE`. SQLite cannot provide MySQL's
assignment ordering, alias diagnostics, warning timing, strict-mode behavior,
rows-matched limit semantics, no-op affected-row accounting, or
order-sensitive uniqueness behavior.

Recommended execution model:

1. Resolve the target table and load column/index metadata.
2. Bind assignments in source order.
3. Bind the optional `WHERE` predicate.
4. Bind optional `ORDER BY` expressions against pre-update row values.
5. Validate and normalize optional `LIMIT row_count`.
6. Determine the union of physical columns required by `WHERE`, `ORDER BY`,
   assignment expressions, assigned columns, generated-column hooks, and unique
   checks.
7. Read candidate physical rows from SQLite without applying SQLite mutation
   semantics.
8. Evaluate `WHERE` on pre-update row values; keep only true rows.
9. Evaluate `ORDER BY` keys on matched pre-update rows, append diagnostics in
   MySQL order, and sort matched rows when requested.
10. Apply `LIMIT` as a rows-matched restriction.
11. For each selected row in update order, build a candidate row initialized
    from the current stored row.
12. Evaluate assignments left to right, mutating the candidate row after each
    assignment.
13. Apply defaults, type conversion, nullability, generated-column hooks,
    unique/primary checks, and auto-increment sequence changes.
14. Write changed rows to SQLite inside one statement transaction.
15. Set statement affected rows to changed rows, retain matched-row count
    internally, and update warning diagnostics.

For no `ORDER BY`, MySQL does not provide a portable row update order
guarantee. MyLite tests must avoid relying on a no-order update sequence unless
the SQL semantics make the outcome order-insensitive.

### Assignment value handling

The candidate row starts with the stored pre-update row values. For each
assignment:

- `DEFAULT` requests the target column default.
- A defaulted nullable column with no explicit default receives `NULL`.
- A `NOT NULL` column with an explicit default receives that default.
- A `NOT NULL` non-auto column with no explicit default fails with 1364 when
  assigned `DEFAULT`.
- An `AUTO_INCREMENT` column assigned `DEFAULT` stores `0`; it does not
  generate a sequence value.
- An `AUTO_INCREMENT` column assigned `NULL` fails with 1048 in the verified
  strict mode.
- An `AUTO_INCREMENT` column assigned an explicit nonzero high value advances
  the table's next generated value after statement success.
- Explicit `NULL` assigned to a required non-auto column fails with 1048 in
  strict mode.
- In non-strict mode, explicit `NULL` assigned to a required non-auto column
  stores the column's implicit type default and records a warning.

Expression evaluation should use the Task 16 value model and conversion rules.
Full strict-mode promotion of assignment, predicate, and order-expression
conversion warnings to data-change errors is deferred until the value/type
foundations can support it consistently. Non-strict warning demotion depends on
future SQL-mode state.

### ORDER BY and LIMIT execution

`ORDER BY` keys are evaluated against pre-update row values after `WHERE`
filtering and before `LIMIT`. Warnings or strict-mode errors from order
expressions are statement-visible even for rows that a later `LIMIT` would
exclude, because ordering needs all matched rows.

`LIMIT row_count`:

- accepts only an unsigned integer literal in direct SQL execution
- accepts `18446744073709551615`
- rejects offset forms, negative literals, decimal literals, quoted strings,
  `NULL`, and arithmetic expressions as syntax errors
- restricts matched rows, not changed rows
- can produce changed rows `0` when the limited rows are no-ops

Future prepared-statement marker behavior belongs to Task 42. Task 19 should
not accept `LIMIT ?` in direct text execution.

### Affected rows, warnings, and diagnostics

The statement should track:

- matched rows
- changed rows
- warning count
- first error condition
- warning records in MySQL-observed order

`mylite_affected_rows()` should return changed rows for the default client
behavior. The executor should keep matched rows in the statement result state
so future protocol and `CLIENT_FOUND_ROWS` support can expose matched-row
affected counts and MySQL-style info strings.

Required target diagnostics for Task 19:

| Code | SQLSTATE | Condition |
| --- | --- | --- |
| 1046 | `3D000` | no selected schema |
| 1048 | `23000` | explicit `NULL` for required column |
| 1049 | `42000` | unknown explicit schema |
| 1054 | `42S22` | unknown assignment, expression, predicate, or order column |
| 1062 | `23000` | duplicate primary or unique key after update |
| 1064 | `42000` | syntax errors, including invalid `LIMIT` forms |
| 1146 | `42S02` | missing table in existing schema |
| 1292 | `22007` | truncated numeric conversion in update expressions |
| 1364 | `HY000` | `DEFAULT` for required column with no default |
| 1365 | `22012` | division by zero in data-change expressions |
| 1366 | `HY000` | incorrect value for target column |
| 3105 | `HY000` | non-`DEFAULT` assignment to generated column, once generated columns exist |

If the current public API is still message-only when Task 19 is implemented,
the runtime should still store structured code, SQLSTATE, and warning records
internally, then document any exposure gap in `COMPATIBILITY.md`.

### Metadata, catalog, and storage impact

Single-table `UPDATE` produces no result set. Result column metadata is not
created.

Persistent effects:

- user table rows may change
- SQLite-maintained primary/unique/secondary indexes must reflect changed rows
- `__mylite_table_catalog.AUTO_INCREMENT` may advance after successful updates
  to an auto-increment column
- schema, table, column, and index catalog definitions do not otherwise change
- the `.mylite` file format does not change

Session effects:

- statement affected rows becomes changed rows
- warning/error diagnostics describe the last statement
- ordinary updates leave session last insert id unchanged
- `LAST_INSERT_ID(expr)` inside an update expression changes the session value
  once that function is supported

### Transactions and atomicity

Task 19 must execute as one atomic statement. On validation, expression,
conversion, duplicate-key, nullability, generated-column, binding, allocation,
or SQLite failure:

- no physical row changes from the failed statement remain
- statement affected rows should report failure behavior compatible with MySQL
- warning/error diagnostics for the failed statement remain available
- `AUTO_INCREMENT` catalog changes from the failed update should not commit
  unless a later MySQL-runtime probe proves a specific sequence side effect
  must survive rollback

The first implementation can use a SQLite transaction or savepoint around the
statement. When explicit transaction statements land later, the update executor
should use a nested savepoint so statement rollback does not roll back the
user's surrounding transaction.

### SQLite-vs-MySQL semantic risks

- SQLite assignment evaluation does not guarantee MySQL's left-to-right
  repeated-target behavior.
- SQLite `UPDATE ... ORDER BY ... LIMIT` support, when compiled in, has
  different accepted syntax and does not provide MySQL diagnostics.
- SQLite affected rows do not distinguish MySQL changed rows, matched rows,
  and no-op assignment behavior in the required way.
- SQLite type affinity and constraint errors do not match MySQL strict and
  non-strict conversion diagnostics.
- SQLite conflict handling does not model MySQL's order-sensitive uniqueness
  examples or MySQL error messages.
- SQLite has no knowledge of MyLite's `AUTO_INCREMENT` catalog state,
  generated-column compatibility contract, or session last insert id.

Use SQLite for durable storage, not as the semantic authority for Task 19.

## Explicit deferred behavior

- CTEs, correlated subqueries, subquery predicates outside the currently
  supported scalar-expression contexts, and self-referencing subquery
  diagnostics are deferred. Multiple-table joined updates are tracked separately in
  [UPDATE JOIN](../update-join/specs.md).
- `LOW_PRIORITY`, joined `UPDATE IGNORE`, partition clauses, and optimizer hints
  are deferred. Index hints are parsed and ignored by
  [index hint parser placeholders](../index-hint-placeholders/specs.md).
- Remaining unsupported function calls, including `LAST_INSERT_ID(expr)` and
  `DEFAULT(col_name)`, are deferred until their function slices land in DML
  expression contexts.
- Generated-column DDL/runtime behavior is deferred. Task 19 should include
  hooks and diagnostics rather than storing explicit values into generated
  columns.
- Automatic `ON UPDATE CURRENT_TIMESTAMP` refresh is implemented for supported
  `TIMESTAMP` and `DATETIME` columns during single-table `UPDATE`; MyLite uses
  a statement-stable UTC timestamp, skips refresh for no-op rows, and suppresses
  refresh when the column is explicitly assigned by the statement.
- Full type conversion, range clipping, string truncation, temporal validation,
  collation coercibility, and non-default SQL modes are deferred except where
  already implemented by the value/type foundations.
- `CLIENT_FOUND_ROWS`, protocol OK-packet info strings, and exact matched-row
  client capability handling are deferred, but Task 19 should keep the internal
  matched-row count.
- Triggers, foreign keys, cascades, check constraints, views, privileges,
  replication safety, and binary logging are deferred.
- Optimizer pushdown, index selection, stable no-order update order, and
  top-N update optimizations are deferred.

## MySQL-runtime-verified test expectations

Implementation tests should compare MyLite against MySQL 8.4.9 for at least
these cases.

### Parser tests

| SQL | Expected parser outcome |
| --- | --- |
| `UPDATE t SET a = 1` | accepted |
| `UPDATE schema.t SET a = 1` | accepted |
| `UPDATE t AS tt SET tt.a = tt.a + 1 WHERE tt.id = 1` | accepted |
| `UPDATE t tt SET tt.a = tt.a + 1 WHERE tt.id = 1` | accepted |
| `UPDATE schema.t SET t.a = t.a + 1, schema.t.b = t.a WHERE id = 1` | accepted |
| `UPDATE t SET a = DEFAULT, b = a + 1 WHERE id = 1 ORDER BY b DESC LIMIT 1` | accepted |
| `UPDATE t SET a = (SELECT v FROM u WHERE id = 1) WHERE id = 1` | accepted |
| `UPDATE t SET a = 1, a = a + 1 WHERE id = 1` | accepted; duplicate assignment targets are valid |
| `UPDATE t SET a = 1 LIMIT 1` | accepted |
| `UPDATE t SET a = 1 LIMIT 1 OFFSET 1` | syntax error 1064 |
| `UPDATE t SET a = 1 LIMIT 1,1` | syntax error 1064 |
| `UPDATE t SET a = 1 LIMIT -1` | syntax error 1064 |
| `UPDATE t SET a = 1 LIMIT 1.5` | syntax error 1064 |
| `UPDATE t SET a = 1 LIMIT '1'` | syntax error 1064 |
| `UPDATE t SET a = 1 LIMIT NULL` | syntax error 1064 |
| `UPDATE t SET a = 1 LIMIT 18446744073709551616` | syntax error 1064 |
| `UPDATE LOW_PRIORITY t SET a = 1` | deferred unless modifier support is implemented |
| `UPDATE IGNORE t SET a = 1` | accepted for the single-table first slice |
| `UPDATE IGNORE t JOIN u ON t.id = u.id SET t.a = u.a` | deferred until joined `UPDATE IGNORE` support |
| `WITH cte AS (SELECT 1) UPDATE t SET a = 1` | deferred until CTE support |
| `UPDATE t JOIN u ON t.id = u.id SET t.a = u.a` | covered by the separate UPDATE JOIN feature |

### Basic updates and assignment order

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `UPDATE t SET a = a + 1 WHERE id = 10` | changes one row; affected rows `1` |
| `UPDATE t SET a = 999 WHERE id = 999` | matches zero; affected rows `0` |
| `UPDATE t SET a = a WHERE id IN (10,11)` | matches two, changes zero; affected rows `0` |
| `UPDATE t SET a = a + 1, b = a WHERE id = 10` | stores updated `a` into `b` |
| `UPDATE t SET a = 100, a = a + 1 WHERE id = 11` | accepts repeated target and stores `101` |
| `UPDATE t SET b = a, a = a + 1 WHERE id = 10` | `b` sees the old `a` because assignment order is reversed |
| `UPDATE t SET s = (SELECT label FROM lookup WHERE id = 1) WHERE id = 10` | assigns the scalar subquery value |

### Target aliases and column resolution

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `UPDATE t AS tt SET tt.a = tt.a + 10 WHERE tt.id = 12` | succeeds |
| `UPDATE t tt SET tt.a = tt.a + 10 WHERE tt.id = 12` | succeeds |
| `UPDATE t AS tt SET t.a = 1 WHERE tt.id = 12` | error 1054, unknown `t.a` in `field list` |
| `UPDATE t AS tt SET tt.a = 1 WHERE t.id = 12` | error 1054, unknown `t.id` in `where clause` |
| `UPDATE t AS tt SET tt.a = 1 ORDER BY t.id LIMIT 1` | error 1054, unknown `t.id` in `order clause` |
| `UPDATE schema.t AS tt SET schema.t.a = 1 WHERE tt.id = 1` | error 1054, alias hides schema-qualified base name |
| `UPDATE schema.t SET t.a = t.a + 1, schema.t.b = t.a WHERE id = 1` | succeeds when no alias is present |
| `UPDATE t SET camelcase = 31 WHERE id = 1` | resolves `CamelCase` case-insensitively |
| `UPDATE t SET missing_col = 1 WHERE id = 1` | error 1054 in `field list` |
| `UPDATE t SET a = missing_col WHERE id = 1` | error 1054 in `field list` |
| `UPDATE t SET a = 1 WHERE missing_col = 1` | error 1054 in `where clause` |
| `UPDATE t SET a = 1 ORDER BY missing_col LIMIT 1` | error 1054 in `order clause` |

### WHERE, ORDER BY, and LIMIT

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `UPDATE t SET a = a + 1 WHERE c IS NULL` | only rows with true predicate match |
| `UPDATE t SET a = a + 1 WHERE 0` | matches zero rows |
| `UPDATE t SET a = a + 1 WHERE NULL` | matches zero rows |
| `UPDATE pk SET id = id + 1` with keys `1,2,3,4` | duplicate primary-key error; table unchanged |
| `UPDATE pk SET id = id + 1 ORDER BY id DESC` | succeeds and changes all rows |
| `UPDATE pk SET v = v + 1` with unique values `1,2,3,4` | duplicate unique-key error; table unchanged |
| `UPDATE pk SET v = v + 1 ORDER BY v DESC LIMIT 2` | changes the two largest `v` rows |
| `UPDATE pk SET note = note ORDER BY v ASC LIMIT 2` | matches two rows, changes zero rows |
| `UPDATE pk SET note = 'x' WHERE v >= 5 ORDER BY v DESC LIMIT 1` | filters, orders, and changes one row |
| `UPDATE t SET a = a + 1 LIMIT 0` | matches zero rows and changes zero rows |
| `UPDATE t SET a = a + 1 LIMIT 18446744073709551615` | accepted and can match all rows |

### Defaults, nullability, and generated columns

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `UPDATE t SET a = DEFAULT, c = DEFAULT, nn = DEFAULT WHERE id = 13` | stores explicit default, `NULL`, and default `7` |
| `UPDATE t SET must = DEFAULT WHERE id = 13` | error 1364; row unchanged |
| `UPDATE t SET nn = NULL WHERE id = 1` in default strict mode | error 1048; row unchanged |
| same `nn = NULL` in non-strict mode | warning 1048 and implicit numeric default `0`, once SQL modes are supported |
| `UPDATE g SET a = a + 5 WHERE id = 1` for generated columns | generated values recompute; deferred until generated columns exist |
| `UPDATE g SET b = DEFAULT, c = DEFAULT WHERE id = 1` | accepted no-op for generated columns; deferred until generated columns exist |
| `UPDATE g SET b = 99 WHERE id = 1` | error 3105; deferred until generated columns exist |
| automatic `ON UPDATE CURRENT_TIMESTAMP` column refresh | supported for changed rows; no-op rows preserve stored values; explicit assignments suppress automatic refresh |

### Type conversion and warnings

| SQL | Expected MyLite-compatible outcome |
| --- | --- |
| `UPDATE w SET a = 'abc' WHERE id = 1` in default strict mode | error 1366; table unchanged |
| `UPDATE w SET a = z + 0 WHERE id IN (2,3)` in default strict mode | error 1292; table unchanged |
| `UPDATE w SET a = a + 1 WHERE z = 2` in default strict mode | error 1292; table unchanged |
| `UPDATE w SET a = a + 1 ORDER BY z + 0, id LIMIT 2` in default strict mode | error 1292; table unchanged |
| `UPDATE w SET a = 1 / 0 WHERE id = 1` in default strict mode | error 1365; table unchanged |
| `UPDATE w SET a = a + 1 WHERE 1 / 0` in default strict mode | error 1365; table unchanged |
| non-strict `UPDATE w SET a = a + 1 WHERE z = 2` | succeeds with two 1292 warnings; SQL-mode support deferred if unavailable |
| non-strict `UPDATE w SET a = z + 0 WHERE id IN (2,3)` | succeeds with two 1292 warnings; SQL-mode support deferred if unavailable |

### AUTO_INCREMENT and last insert id

| SQL or sequence | Expected MyLite-compatible outcome |
| --- | --- |
| insert generated ids `5,6,7`, then ordinary `UPDATE ai SET v = v + 1 WHERE id = 5` | `LAST_INSERT_ID()` remains `5`; affected rows `1` |
| `UPDATE ai SET id = 100 WHERE id = 6`; then `INSERT INTO ai (v) VALUES (40)` | next generated id is `101` |
| `UPDATE ai SET id = 0 WHERE id = 5` | stores zero; `LAST_INSERT_ID()` unchanged |
| `UPDATE ai SET id = DEFAULT WHERE v = 1` | stores zero; does not generate a new id |
| `UPDATE ai SET id = NULL WHERE v = 40` | error 1048; row unchanged |
| `UPDATE seq SET id = LAST_INSERT_ID(id + 1)` | updates the session last insert id once function support exists; otherwise deterministic unsupported-function diagnostic |

### Diagnostics and lifecycle

| SQL sequence | Expected MyLite-compatible outcome |
| --- | --- |
| failed update, then `SHOW COUNT(*) WARNINGS` | returns `1` for the error condition |
| failed update, then `SHOW WARNINGS` | reports the error code/message for the failed update |
| `SHOW COUNT(*) WARNINGS` immediately after `SHOW WARNINGS` | warning count is unchanged |
| warning-producing non-strict update, then `SHOW WARNINGS` | reports warning rows in MySQL order |
| later nondiagnostic statement after warning/error update | clears the previous warning list |
| failed update, then `ROW_COUNT()` | returns `-1` |

### Metadata and side effects

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| successful `UPDATE` | no result set and no result-column metadata |
| successful row-changing update | physical table rows and SQLite indexes reflect new values |
| no-op update | matched rows tracked internally; changed rows and affected rows are `0` |
| failed update after some candidate rows were evaluated | no physical row changes remain |
| successful explicit high auto-increment update | table catalog next sequence advances |
| ordinary successful update | schema/table/column/index catalog definitions unchanged |
| ordinary successful update | session last insert id unchanged |

## Test plan

Parser tests:

- base `UPDATE t SET a = 1`
- schema-qualified targets
- aliases with and without `AS`
- unqualified, table-qualified, schema-table-qualified, and alias-qualified
  assignment targets
- repeated assignment targets
- assignment values using literals, identifiers, arithmetic, `NULL`, and
  `DEFAULT`
- optional `WHERE`, `ORDER BY`, and `LIMIT row_count`
- valid upper-bound limit literal `18446744073709551615`
- syntax rejection for offset limits, negative/decimal/string/`NULL` limits,
  missing assignment list, missing target, missing value, trailing comma, and
  invalid clause order
- deferred-form rejection or unsupported diagnostics for `LOW_PRIORITY`,
  joined `UPDATE IGNORE`, partitions, joins, CTEs, subqueries, function calls, and
  `DEFAULT(col_name)`

Runtime tests:

- target resolution through selected schema and schema-qualified names
- missing schema, missing table, and no selected schema diagnostics
- alias-visible and alias-hidden qualifiers in assignment, `WHERE`, and
  `ORDER BY`
- case-insensitive column lookup for assignment targets and expressions
- assignment source order with old/new column values
- repeated assignment targets
- unknown assignment targets and unknown expression columns
- `WHERE` filtering with true, false, and `NULL` predicates
- order-sensitive primary-key and unique-key updates
- `LIMIT` rows-matched behavior, including no-op limited updates
- no-order tests only where final results are order-insensitive
- `DEFAULT` for literal defaults, nullable columns, required no-default
  columns, and auto-increment columns
- explicit `NULL` into required columns
- `AUTO_INCREMENT` explicit high updates, zero/default/null behavior, sequence
  advancement, and last insert id preservation
- generated-column target diagnostics once generated columns exist
- warning-list lifecycle after successful warning-producing and failed updates
- affected rows, matched rows, warning count, and failure `ROW_COUNT()`
- atomic rollback after duplicate-key, nullability, unsupported expression, and
  SQLite binding failures

Deferred conversion tests before closing the documented conversion gap:

- strict-mode predicate, assignment, and order-expression conversion errors
- non-strict conversion warnings when SQL-mode support is available

## Implementation notes

- Parser/AST support landed before runtime execution, matching this feature's
  grammar snippets.
- Reuse the table resolution and row context from Tasks 15 through 18, but keep
  update binding separate because assignment target diagnostics use
  `field list`.
- Preserve assignment source order all the way into execution. Do not dedupe
  targets.
- Use a MyLite row executor for the initial runtime. SQLite can store the final
  row changes, but it should not decide UPDATE semantics.
- Track matched rows separately from changed rows from the first
  implementation.
- Add broader strict-mode conversion tests before adding any SQLite pushdown or
  top-N optimization.
- Use statement-scoped atomic execution so failed updates roll back physical
  changes without damaging future explicit transaction support.
- Keep generated-column, `ON UPDATE`, function-call, and non-default SQL-mode
  behavior explicit in unsupported/deferred diagnostics until those foundations
  land.
