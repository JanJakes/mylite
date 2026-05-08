# Single-table DELETE

## Scope

This feature specifies Task 20, executable single-table `DELETE` for user base
tables created by MyLite's supported `CREATE TABLE` subset. It builds on the
Task 15 single-table scan, Task 16 expression evaluator, Task 17 predicate
binder, Task 18 ordering/windowing design, and Task 13/14 write diagnostics.

In scope:

- `DELETE FROM table_name`
- schema-qualified targets and selected-schema target resolution
- target aliases using `AS alias` or a bare identifier alias
- `WHERE` filtering over the Task 17 predicate subset
- `ORDER BY` over the Task 18 order-expression subset
- `LIMIT row_count` as a deleted-row restriction
- deletion of all rows when `WHERE` and `LIMIT` are omitted
- affected rows, warning counts, row-count diagnostics, and failed-statement
  diagnostics
- atomic rollback for binding, expression, conversion, storage, and
  constraint failures
- physical deletion from SQLite tables plus preservation of MyLite catalog rows
- `AUTO_INCREMENT` side effects after deleting ordinary rows, maximum-id rows,
  and all rows
- design hooks for generated columns, triggers, foreign keys, and cascading
  actions, while their execution remains deferred

Out of scope for Task 20:

- multiple-table `DELETE` forms using `FROM` or `USING`
- common table expressions before `DELETE`
- `LOW_PRIORITY`, `QUICK`, and `IGNORE`
- partition clauses, table-sampling syntax, optimizer hints, and locking
  modifiers
- `ORDER BY` or `LIMIT` variants other than single `LIMIT row_count`
- subqueries, `EXISTS`, row constructors, variables, parameters, function
  calls, casts, `CASE`, collations, JSON operators, regular expressions, and
  other expression shapes outside Task 16
- delete-through-view behavior
- generated-column execution effects beyond ordinary predicate/order
  expression dependencies that are already represented as stored values
- triggers, foreign keys, cascading actions, check constraints, privileges,
  binary logging, replication safety, and optimizer plan details
- full strict/non-strict predicate and order-expression conversion behavior
  beyond the existing expression/value foundations
- full SQL-mode management beyond the verified default strict mode and the
  documented non-strict warning baseline

Task 20 is implemented for the executable subset above: parser, AST, analyzer,
runtime deletion, affected rows, deterministic diagnostics, rollback, tests,
and compatibility documentation are shipped together. Deferred MySQL grammar
and semantic surfaces remain explicitly tracked below.

## Sources

- MySQL 8.4 Reference Manual, `DELETE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/delete.html
- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- MySQL 8.4 Reference Manual, `AUTO_INCREMENT` Handling in InnoDB:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-auto-increment-handling.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Existing MyLite specs:
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
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
DROP DATABASE IF EXISTS mylite_task20_delete;
CREATE DATABASE mylite_task20_delete;
USE mylite_task20_delete;

CREATE TABLE t (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  category INT,
  v INT,
  s VARCHAR(20),
  nullable INT NULL,
  CamelCase INT,
  KEY idx_v (v)
) AUTO_INCREMENT=10;

INSERT INTO t (category, v, s, nullable, CamelCase) VALUES
  (1, 10, 'alpha', NULL, 100),
  (1, 30, 'beta', 5, 200),
  (2, 20, 'gamma', NULL, 300),
  (2, 40, 'delta', 0, 400),
  (3, 50, 'epsilon', 7, 500);
```

Additional focused fixtures covered target-resolution errors, strict and
non-strict conversion behavior, warning lifecycle, invalid `LIMIT` syntax, and
`AUTO_INCREMENT` side effects.

### Target resolution

Single-table `DELETE` removes rows from one target table. With no `WHERE`
clause and no `LIMIT`, every row in the table is deleted. The target may be
schema-qualified or resolved against the selected default schema.

Representative target-resolution behavior:

| SQL or condition | MySQL behavior |
| --- | --- |
| no selected schema for unqualified `DELETE FROM t ...` | error 1046 / `3D000`, `No database selected` |
| `DELETE FROM missing_schema.t ...` | error 1049 / `42000`, unknown database |
| `DELETE FROM existing_schema.missing_t ...` | error 1146 / `42S02`, table does not exist |
| `DELETE FROM existing_schema.t WHERE id = 1` | succeeds when the table exists |

MyLite should reject writes to system schemas and catalog tables through the
same policy used by other DML targets. The official MySQL privilege check is a
server concern and is deferred until MyLite has a privilege model.

### Aliases and column resolution

MySQL accepts target aliases using `AS` or a bare identifier:

```sql
DELETE FROM t AS tt WHERE tt.id = 10;
DELETE FROM t tt WHERE tt.id = 11;
```

When an alias is present, the alias is the visible table qualifier. The base
table name and schema-qualified base-table name are hidden from `WHERE` and
`ORDER BY` resolution.

| SQL | MySQL behavior |
| --- | --- |
| `DELETE FROM t AS tt WHERE tt.id = 10` | succeeds; `ROW_COUNT()` is `1` |
| `DELETE FROM t tt WHERE tt.id = 11` | succeeds; `ROW_COUNT()` is `1` |
| `DELETE FROM t AS tt WHERE t.id = 12` | error 1054 / `42S22`, unknown column `t.id` in `where clause` |
| `DELETE FROM schema.t AS tt WHERE schema.t.id = 1` | error 1054 / `42S22`, unknown column in `where clause` |
| `DELETE FROM t AS tt ORDER BY t.v LIMIT 1` | error 1054 / `42S22`, unknown column `t.v` in `order clause` |

Without an alias, predicate and order expressions may use unqualified,
table-qualified, or schema-table-qualified column references:

```sql
DELETE FROM mylite_task20_delete.t
WHERE mylite_task20_delete.t.id = 12;
```

Column-name lookup is case-insensitive for the verified identifier repertoire.
Schema, table, and alias qualifier matching follows the existing MyLite policy
from table-backed `SELECT`: byte-preserving and case-sensitive on the verified
Linux MySQL runtime.

### WHERE interaction

The optional `WHERE` clause filters candidate rows before ordering, limiting,
and deletion. Only rows whose predicate is true are selected for deletion.
False and `NULL` predicates do not match.

Name resolution in `WHERE` follows Task 17 rules with a delete-specific target
table context:

| SQL | MySQL behavior |
| --- | --- |
| `DELETE FROM t WHERE id = 10` | deletes row `10` |
| `DELETE FROM t AS tt WHERE tt.id = 10` | alias-qualified predicate succeeds |
| `DELETE FROM t AS tt WHERE t.id = 10` | error 1054 / `42S22`, unknown column in `where clause` |
| `DELETE FROM t WHERE missing_col = 1` | error 1054 / `42S22`, unknown column in `where clause` |

Predicate conversion behavior follows data-change statement rules. Under the
verified default strict SQL mode:

```sql
CREATE TABLE w (id INT PRIMARY KEY, v INT, z VARCHAR(20));
INSERT INTO w VALUES (1,10,'2'),(2,20,'2a'),(3,30,'a'),(4,40,'10');
DELETE FROM w WHERE z = 2;
```

The statement fails with error 1292 / `22007`,
`Truncated incorrect DOUBLE value: '2a'`, records one error condition in the
diagnostics area, and leaves all rows unchanged. `ROW_COUNT()` after the failed
statement is `-1`.

With `SET SESSION sql_mode = ''`, the same predicate succeeds, deletes rows
with `z='2'` and `z='2a'`, reports `ROW_COUNT() = 2`, and records two 1292
warnings for `'2a'` and `'a'`.

### ORDER BY and LIMIT interaction

For single-table `DELETE`, `ORDER BY` determines the deletion order. `LIMIT
row_count` restricts the number of rows deleted. Filtering happens first,
ordering happens over the matched rows, and `LIMIT` is applied to the ordered
matched rows.

Representative runtime result:

```sql
INSERT INTO t (category, v, s, nullable, CamelCase) VALUES
  (1, 10, 'a', NULL, 100),
  (1, 30, 'b', 5, 200),
  (1, 20, 'c', NULL, 300),
  (1, 40, 'd', 0, 400);

DELETE FROM t WHERE category = 1 ORDER BY v DESC, id ASC LIMIT 2;
```

The statement deletes the two matching rows with the largest `v` values. In
the observed fixture, rows with `v=40` and `v=30` were deleted and the remaining
category `1` rows had `v=20` and `v=10`.

`ORDER BY` expressions are evaluated over pre-delete row values and target
table columns. There are no projection aliases in `DELETE`, so Task 18's
projection-alias lookup rules do not apply.

`LIMIT` syntax observations:

| SQL | MySQL behavior |
| --- | --- |
| `DELETE FROM t LIMIT 0` | accepted; deletes zero rows |
| `DELETE FROM t LIMIT 18446744073709551615` | accepted |
| `DELETE FROM t LIMIT -1` | syntax error 1064 / `42000` |
| `DELETE FROM t LIMIT '1'` | syntax error 1064 / `42000` |
| `DELETE FROM t LIMIT 1 OFFSET 1` | syntax error 1064 / `42000` |
| `DELETE FROM t LIMIT 1, 1` | syntax error 1064 / `42000` |
| `DELETE FROM t LIMIT 18446744073709551616` | syntax error 1064 / `42000` |

Under non-strict SQL mode, warning-producing order expressions can still
succeed. For example, `DELETE FROM w ORDER BY z + 0, id LIMIT 2` deleted two
rows and recorded two 1292 warnings for `'2a'` and `'a'`. Under the default
strict mode, warning-producing expressions in a data-change statement can be
promoted to errors, matching the predicate behavior above.

Without `ORDER BY`, MySQL does not provide a portable row order guarantee.
MyLite tests must not depend on which rows are deleted by `DELETE ... LIMIT n`
unless an `ORDER BY` clause makes the row set deterministic.

### Affected rows, diagnostics, and warnings

MySQL reports deleted rows as the affected-row value. There is no separate
changed-versus-matched distinction for `DELETE`.

Representative runtime results:

| SQL | `ROW_COUNT()` / affected rows |
| --- | --- |
| `DELETE FROM t WHERE id = 10` | `1` |
| `DELETE FROM t WHERE id = 999` | `0` |
| `DELETE FROM t WHERE category = 1 LIMIT 0` | `0` |
| `DELETE FROM t WHERE category = 1 ORDER BY v DESC LIMIT 2` | `2` |
| failed semantic or syntax error, then immediate `SELECT ROW_COUNT()` | `-1` |

The diagnostics area should keep the error condition after failed `DELETE`.
`SHOW COUNT(*) WARNINGS` and `SHOW WARNINGS` inspect that list without clearing
it. A later nondiagnostic result-producing statement, such as `SELECT
ROW_COUNT()`, can replace the previous diagnostics state, so tests that need
warning rows should inspect them before running unrelated `SELECT` statements.

### Atomicity and side effects

For the supported single-table subset, a successful `DELETE` physically removes
the selected rows and leaves table, column, index, and schema catalog rows
intact. A failed statement must leave the physical table unchanged.

Observed strict-mode failure:

```sql
DELETE FROM w WHERE z = 2;
```

When `z` contains `'2a'`, MySQL reports error 1292 and all original rows remain
in the table. MyLite must preserve statement atomicity for binding,
expression, conversion, storage, trigger, foreign-key, and SQLite failures.

`DELETE` does not change `LAST_INSERT_ID()`. It does affect subsequent
`AUTO_INCREMENT` allocation only through the storage engine's sequence rules.
For an InnoDB table created as:

```sql
CREATE TABLE ai (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  v INT
) AUTO_INCREMENT=100;
INSERT INTO ai (v) VALUES (1),(2),(3);
```

Observed behavior:

| SQL sequence | MySQL behavior |
| --- | --- |
| delete row `id=102`; insert one new row | new row receives `id=103` |
| `DELETE FROM ai`; insert one new row | new row receives `id=104` |
| `LAST_INSERT_ID()` after those inserts | reports the generated insert id, not the preceding delete |

For MyLite's initial single-file table storage, the practical compatibility
target is InnoDB-like persistence of the table's next `AUTO_INCREMENT` value:
deleting the maximum row or all rows must not rewind the sequence. MyISAM
multi-column sequence reuse is explicitly deferred.

### Deferred interactions

Generated columns:

- A `DELETE` may read generated column values in `WHERE` or `ORDER BY` once
  generated columns exist in MyLite's catalog and row materialization path.
- Deleting a row does not compute replacement values, but generated-column
  metadata may affect dependencies and trigger expressions later.

Triggers:

- MySQL fires `BEFORE DELETE` and `AFTER DELETE` triggers once trigger support
  exists.
- Task 20 should include a runtime hook point but must not implement trigger
  parsing or execution.

Foreign keys:

- MySQL enforces referenced-row restrictions and cascading actions for
  `DELETE`.
- Task 20 should include a constraint hook point and preserve statement
  atomicity, but foreign-key checks and cascades remain Task 48 work.

Privileges, binary logging, replication, partition pruning, server table
locking, and storage-engine-specific optimizations are server-only or later
features for MyLite and must remain deferred.

## MyLite behavior

### Parser and AST

Add a single-table `delete_statement` AST node that records:

1. target table name
2. optional target alias
3. optional `WHERE` clause
4. optional `ORDER BY` clause
5. optional `LIMIT` clause

Recommended AST additions:

- `MYLITE_SQL_AST_DELETE_STATEMENT`
- `MYLITE_SQL_AST_DELETE_TARGET`
- `MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE`

The delete target node should store the source-spanned table name and optional
alias. The statement should preserve clause source order and spans for precise
syntax and semantic diagnostics.

### Lemon grammar snippets

These snippets describe MyLite's intended Task 20 grammar shape. They are not
copied from MySQL grammar.

```lemon
delete_statement ::= DELETE FROM single_delete_target opt_where_clause
    opt_order_by_clause opt_delete_limit_clause.

single_delete_target ::= table_name opt_table_alias.

opt_table_alias ::= .
opt_table_alias ::= table_alias.
opt_table_alias ::= AS table_alias.

table_alias ::= identifier.

opt_where_clause ::= .
opt_where_clause ::= WHERE expression.

opt_order_by_clause ::= .
opt_order_by_clause ::= ORDER BY delete_order_item_list.

delete_order_item_list ::= delete_order_item.
delete_order_item_list ::= delete_order_item_list COMMA delete_order_item.

delete_order_item ::= expression opt_order_direction.

opt_order_direction ::= .
opt_order_direction ::= ASC.
opt_order_direction ::= DESC.

opt_delete_limit_clause ::= .
opt_delete_limit_clause ::= LIMIT unsigned_integer_literal.
```

`WHERE`, `ORDER`, and `LIMIT` are clause boundaries. Bare table aliases must
not consume those keywords as aliases.

The following MySQL-supported grammar surface remains outside Task 20:

```lemon
/* Deferred: modifiers. */
delete_statement ::= DELETE LOW_PRIORITY FROM single_delete_target.
delete_statement ::= DELETE QUICK FROM single_delete_target.
delete_statement ::= DELETE IGNORE FROM single_delete_target.

/* Deferred: common table expressions. */
delete_statement ::= WITH cte_list DELETE FROM single_delete_target.

/* Deferred: partition selection. */
single_delete_target ::= table_name opt_table_alias
    PARTITION LPAREN identifier_list RPAREN.

/* Deferred: multiple-table deletes. */
delete_statement ::= DELETE delete_target_list FROM table_reference_list
    opt_where_clause.
delete_statement ::= DELETE FROM delete_target_list USING table_reference_list
    opt_where_clause.

/* Deferred: unsupported DELETE LIMIT forms. */
opt_delete_limit_clause ::= LIMIT unsigned_integer_literal COMMA
    unsigned_integer_literal.
opt_delete_limit_clause ::= LIMIT unsigned_integer_literal OFFSET
    unsigned_integer_literal.
```

### Binding and name resolution

The delete binder should reuse the single-table resolution machinery from
Tasks 15 through 18 with a delete-specific diagnostic context.

Target binding:

- Resolve `schema.table` against the written schema.
- Resolve unqualified `table` against the selected default schema.
- Reject missing selected schema, missing explicit schema, missing table, and
  system-schema targets before predicate or order binding.
- Load column metadata from `__mylite_column_catalog` in ordinal order.
- Load primary and unique index metadata needed for future foreign-key and
  trigger hook decisions.
- Load table-level `AUTO_INCREMENT` state from `__mylite_table_catalog`.

Visible table identity:

- If an alias exists, only the alias is a valid two-part qualifier.
- If no alias exists, the table name is a valid two-part qualifier.
- If no alias exists, `schema.table.column` is valid when schema and table
  match the resolved target.
- If an alias exists, base table and schema-qualified base table qualifiers are
  hidden for `WHERE` and `ORDER BY` resolution.

Column lookup:

- Match column names case-insensitively.
- Match schema/table/alias qualifiers according to the existing
  byte-preserving case-sensitive schema/table policy.
- Unknown predicate references use the 1054-style `where clause` diagnostic.
- Unknown order references use the 1054-style `order clause` diagnostic.
- Ambiguous unqualified references are not reachable in Task 20's one-table
  scope, but the binder should keep the join-era 1052 diagnostic path
  available.

The predicate and order-expression binders should reject unsupported expression
shapes deterministically unless MySQL itself reports a syntax or semantic
error first.

### Runtime execution model

The first implementation should use a MyLite-owned delete executor rather than
delegating semantics to SQLite `DELETE`. SQLite cannot provide MySQL's alias
diagnostics, strict-mode warning promotion, warning timing, `LIMIT` literal
syntax, collation behavior, future trigger/cascade ordering, or affected-row
and diagnostics lifecycle.

Recommended execution model:

1. Resolve the target table and load column/index/sequence metadata.
2. Bind the optional `WHERE` predicate.
3. Bind optional `ORDER BY` expressions against pre-delete row values.
4. Validate and normalize optional `LIMIT row_count`.
5. Determine the union of physical columns required by `WHERE`, `ORDER BY`,
   trigger hooks, foreign-key hooks, and physical row identity.
6. Read candidate physical rows from SQLite without applying SQLite mutation
   semantics.
7. Evaluate `WHERE` on pre-delete row values; keep only true rows.
8. Evaluate `ORDER BY` keys on matched pre-delete rows, append diagnostics in
   MySQL order, and sort matched rows when requested.
9. Apply `LIMIT` as a deleted-row restriction.
10. Start one statement transaction or savepoint.
11. Run future `BEFORE DELETE` hooks and constraint checks for each selected
    row in deletion order.
12. Delete the selected physical rows by stable row identity.
13. Run future `AFTER DELETE` hooks and cascading side effects when those
    features exist.
14. Commit the statement transaction, set affected rows to the deleted-row
    count, preserve the table's next `AUTO_INCREMENT` value, and update
    warning diagnostics.

For no `ORDER BY`, MySQL does not provide a portable deletion order guarantee.
MyLite may stream rows in storage order only when the visible result is
order-insensitive. Tests must add deterministic `ORDER BY` clauses whenever
`LIMIT` controls which rows are deleted.

### DELETE value handling

`DELETE` does not assign new column values, evaluate defaults, or allocate
`AUTO_INCREMENT` values. It can still evaluate expressions over row values in
`WHERE` and `ORDER BY`.

Expression evaluation should use the Task 16 value model and conversion rules.
In the verified default strict SQL mode, conversion warnings in supported
predicate or order expressions for data-change statements should fail the
statement and roll back all row deletions. Non-strict mode should preserve the
warnings while allowing the statement to complete.

Deleting rows must not:

- change `LAST_INSERT_ID()`
- remove schema/table/column/index catalog rows
- reset the table's `AUTO_INCREMENT` counter
- reuse deleted maximum values for InnoDB-like tables
- expose predicate or order columns as result metadata

### Diagnostics and warnings

Required target diagnostics for Task 20:

- 1046 / `3D000`: no selected database for unqualified target
- 1049 / `42000`: unknown explicit database
- 1054 / `42S22`: `Unknown column 'name' in 'where clause'`
- 1054 / `42S22`: `Unknown column 'name' in 'order clause'`
- 1064 / `42000`: syntax errors for invalid `LIMIT` bounds, unsupported
  offset/comma forms, unsupported modifiers, CTEs before `DELETE`, malformed
  clauses, and multiple-table syntax while it remains unimplemented
- 1146 / `42S02`: target table does not exist
- 1210 / `HY000`: invalid `LIKE ... ESCAPE` arguments inherited from Task 17
- 1292 / `22007`: truncated numeric conversion warnings or strict-mode errors
- 1365 / `22012`: division-by-zero warnings or strict-mode errors

Current MyLite message-only errors should still use the MySQL wording where
the project has no numeric-code surface yet. Internally, the executor should
store structured error and warning records so `SHOW WARNINGS`, protocol
diagnostics, and future client APIs can expose them consistently.

Successful `DELETE` sets affected rows to the number of deleted rows. Failed
`DELETE` sets the row-count function target to `-1`, matching the observed
`ROW_COUNT()` behavior after syntax, binding, and strict conversion failures.

### Storage and performance implications

Task 20 does not change the `.mylite` file format, schema catalog, table
catalog, column catalog, or index catalog. It requires statement-runtime state:

- resolved target table metadata
- bound predicate expression
- bound order-key list
- normalized optional delete limit
- stable physical row identities for selected rows
- pre-delete row values needed by expressions and future hooks
- warning records produced during predicate and order evaluation
- statement savepoint/transaction state for rollback

The first implementation may materialize all rows that pass `WHERE` before
ordering and deleting. That is acceptable for Task 20 because MySQL-compatible
warnings, strict-mode behavior, ordering, and hook ordering are the priority.
Later optimizer work may lower simple predicates or ordered deletes to SQLite
only when tests prove that MySQL-visible behavior is preserved.

Avoid per-row heap churn for common integer, `NULL`, and short string values.
If sort keys, conversion scratch values, or row snapshots allocate, make their
lifetime statement-owned and release them after the statement completes or
rolls back.

## SQLite-vs-MySQL semantic risks

- SQLite accepts broader `LIMIT` expressions and offset forms than single-table
  MySQL `DELETE`.
- SQLite's `DELETE ... ORDER BY ... LIMIT` availability and behavior are
  compile-option dependent and do not match MySQL diagnostics.
- SQLite name resolution does not hide base table qualifiers after a MySQL
  target alias in the same way.
- SQLite has different numeric conversion, truthiness, string comparison,
  collation, division-by-zero, and warning behavior.
- SQLite may physically reuse rowids after deletes; MyLite must preserve its
  MySQL-facing `AUTO_INCREMENT` state independently.
- Future triggers and foreign-key actions require MySQL-compatible hook order,
  diagnostics, and statement atomicity rather than SQLite's native behavior.
- No-order `LIMIT` row choice is not portable. Tests must not accidentally
  freeze SQLite scan order as a MySQL guarantee.

## Explicit deferred behavior

- Multiple-table `DELETE` forms are deferred to a separate DML feature.
- `LOW_PRIORITY`, `QUICK`, and `IGNORE` are deferred. MyLite should reject or
  parse-gate them deterministically until their behavior is specified.
- Partition clauses are deferred until partitioned table metadata exists.
- CTEs, subqueries, variables, prepared markers, functions, casts, `CASE`, JSON
  operators, regular expressions, explicit collations, and broad expression
  support are deferred unless already implemented by earlier tasks.
- Generated-column, trigger, foreign-key, and cascading delete execution is
  deferred, with hook points reserved in the executor.
- Privileges, binary logging, replication safety, table locks, server
  concurrency, `LOW_PRIORITY` scheduling, and storage-engine-specific `QUICK`
  behavior are deferred.
- MyISAM-specific auto-increment reuse in grouped secondary-column sequences is
  deferred.
- Optimizer behavior, index choice, predicate pushdown, and early LIMIT
  termination are deferred unless each case is MySQL-runtime verified.

## MySQL-runtime-verified test expectations

Implementation tests should compare MyLite against MySQL 8.4.9 for at least
these cases. Unless a test uses a deterministic `ORDER BY`, compare final row
sets rather than depending on storage order.

### Parser tests

| SQL | Expected parser outcome |
| --- | --- |
| `DELETE FROM t` | accepted with a `MYLITE_SQL_AST_DELETE_STATEMENT` |
| `DELETE FROM t WHERE id = 1` | accepted |
| `DELETE FROM t AS tt WHERE tt.id = 1` | accepted |
| `DELETE FROM t tt WHERE tt.id = 1` | accepted |
| `DELETE FROM db.t WHERE db.t.id = 1` | accepted |
| `DELETE FROM t WHERE category = 1 ORDER BY v DESC, id LIMIT 2` | accepted |
| `DELETE FROM t LIMIT 0` | accepted |
| `DELETE FROM t LIMIT 18446744073709551615` | accepted |
| `DELETE FROM t LIMIT -1` | syntax error 1064 / `42000` |
| `DELETE FROM t LIMIT '1'` | syntax error 1064 / `42000` |
| `DELETE FROM t LIMIT 1 OFFSET 1` | syntax error 1064 / `42000` |
| `DELETE FROM t LIMIT 1, 1` | syntax error 1064 / `42000` |
| `DELETE LOW_PRIORITY FROM t` | deferred unsupported form |
| `DELETE QUICK FROM t` | deferred unsupported form |
| `DELETE IGNORE FROM t` | deferred unsupported form |
| `DELETE FROM t PARTITION (p0)` | deferred unsupported form |
| `WITH c AS (SELECT 1) DELETE FROM t` | deferred unsupported form |
| `DELETE t FROM t JOIN u ON t.id = u.id` | deferred multiple-table form |

### Target resolution and aliases

| SQL or condition | Expected MyLite-compatible outcome |
| --- | --- |
| unqualified target with no selected schema | error 1046 / `3D000` |
| `DELETE FROM missing_schema.t WHERE id = 1` | error 1049 / `42000` |
| `DELETE FROM existing_schema.missing_t WHERE id = 1` | error 1146 / `42S02` |
| `DELETE FROM existing_schema.t WHERE existing_schema.t.id = 1` | deletes one row |
| `DELETE FROM t AS tt WHERE tt.id = 1` | deletes one row |
| `DELETE FROM t tt WHERE tt.id = 1` | deletes one row |
| `DELETE FROM t AS tt WHERE t.id = 1` | unknown column `t.id` in `where clause` |
| `DELETE FROM schema.t AS tt WHERE schema.t.id = 1` | unknown column in `where clause` |
| `DELETE FROM t AS tt ORDER BY t.v LIMIT 1` | unknown column `t.v` in `order clause` |
| `DELETE FROM t WHERE camelcase = 200` | column lookup is case-insensitive |

### WHERE, ORDER BY, and LIMIT

| SQL | Expected final rows or affected rows |
| --- | --- |
| `DELETE FROM t WHERE id = 10` | affected rows `1`; row `10` gone |
| `DELETE FROM t WHERE id = 999` | affected rows `0`; table unchanged |
| `DELETE FROM t WHERE NULL` | affected rows `0`; table unchanged |
| `DELETE FROM t WHERE category = 1 ORDER BY v DESC, id ASC LIMIT 2` | deletes the two category `1` rows with largest `v` values |
| `DELETE FROM t WHERE category = 1 LIMIT 0` | affected rows `0`; table unchanged |
| `DELETE FROM t ORDER BY v ASC, id ASC LIMIT 1` | deletes the row with smallest `v`, tie-broken by `id` |
| `DELETE FROM t WHERE category = 1 LIMIT 2` | affected rows `2`; final row set is not order-specified unless order-insensitive |

### Conversion, warnings, and rollback

| SQL and mode | Expected MyLite-compatible outcome |
| --- | --- |
| strict `DELETE FROM w WHERE z = 2` with `z='2a'` present | error 1292 / `22007`; `ROW_COUNT()` target `-1`; table unchanged |
| non-strict `DELETE FROM w WHERE z = 2` | deletes rows `z='2'` and `z='2a'`; two 1292 warnings |
| strict `DELETE FROM w WHERE 1 / 0` | division-by-zero strict-mode error; table unchanged |
| non-strict `DELETE FROM w ORDER BY z + 0, id LIMIT 2` | deletes first two numeric-order rows; two 1292 warnings |
| `DELETE FROM t WHERE missing_col = 1` | unknown column in `where clause`; table unchanged |
| `DELETE FROM t ORDER BY missing_col LIMIT 1` | unknown column in `order clause`; table unchanged |

### Affected rows and diagnostics

| SQL sequence | Expected result |
| --- | --- |
| successful one-row delete, then `ROW_COUNT()` | `1` |
| delete matching no rows, then `ROW_COUNT()` | `0` |
| `DELETE ... LIMIT 0`, then `ROW_COUNT()` | `0` |
| failed semantic error, then immediate `ROW_COUNT()` | `-1` |
| failed unknown column, then `SHOW WARNINGS` before unrelated statements | one error row with code 1054 |
| strict conversion failure, then `SHOW WARNINGS` before unrelated statements | one error row with code 1292 |

### AUTO_INCREMENT side effects

| SQL sequence | Expected final behavior |
| --- | --- |
| create `AUTO_INCREMENT=100`; insert ids `100`, `101`, `102`; delete `102`; insert | new id is `103` |
| delete all rows from that table; insert | new id continues to `104`, not `1` |
| inspect `LAST_INSERT_ID()` after a delete-only statement | unchanged from the previous insert-generated value |
| delete missing rows from an auto-increment table | affected rows `0`; sequence unchanged |

## Implementation notes

- The implementation reuses the Task 19 mutation rowset, ordering, and limit
  scaffolding where possible, while keeping `DELETE` free of assignment-list,
  changed-row, and duplicate-key update semantics.
- The executor uses a MyLite-owned scan/evaluate/delete path because
  MySQL-visible diagnostics and warning timing cannot be delegated to SQLite.
- Preserve stable physical row identities before deleting. Future trigger and
  foreign-key hooks will need the old row values in MySQL deletion order.
- Treat `ORDER BY` as semantically important for future referential actions
  even though Task 20 defers foreign keys.
- Keep `AUTO_INCREMENT` state in the MyLite table catalog independent of
  SQLite rowid reuse. Deletes should not decrease it.
- Keep tests focused on user base tables from the supported `CREATE TABLE`
  subset and avoid server-only behavior.
- Keep `COMPATIBILITY.md`, parser tests, runtime tests, and this specification
  in sync as deferred DELETE grammar and semantic surfaces are implemented.
