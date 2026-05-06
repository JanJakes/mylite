# REPLACE

## Scope

This feature specifies MySQL-compatible `REPLACE` behavior for MyLite. This
feature now has a first executable slice for `VALUES`/`VALUE`/`VALUES ROW(...)`
and `SET` sources over supported MyLite base tables. Query-source forms and
advanced table features remain deferred.

In scope for the full feature:

- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name [(column_list)]
  {VALUES | VALUE} (...) [, (...)]`
- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name [(column_list)]
  VALUES ROW(...) [, ROW(...)]`
- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name SET assignment_list`
- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name [(column_list)]
  SELECT ...`
- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name [(column_list)]
  TABLE table_name`
- schema-qualified and selected-schema target resolution
- optional `INTO`
- column-list, all-default row, default-value, required-column, duplicate
  target-column, unknown target-column, and row-arity behavior aligned with the
  existing insert specs
- candidate-row construction for `VALUES`, `SET`, and query-source forms
- primary-key and unique-key conflict detection
- delete-all-conflicting-rows behavior for one source row
- source-order multi-row processing, including later source rows replacing rows
  inserted by earlier source rows
- affected rows, processed-record count, duplicate count, warning count,
  session last insert id, and auto-increment side effects
- statement atomicity and rollback for fatal errors
- deterministic treatment for `LOW_PRIORITY`, deprecated `DELAYED`, and invalid
  `IGNORE`/`HIGH_PRIORITY`
- MyLite Lemon-syntax grammar snippets for the intended grammar

First executable implementation slice:

- implement `REPLACE ... VALUES`, `VALUE`, `VALUES ROW(...)`, and
  `REPLACE ... SET` for user base tables created by MyLite's supported
  `CREATE TABLE` subset
- reuse the current `INSERT ... VALUES` and `INSERT ... SET` candidate-row
  builders, default handling, deterministic scalar expression subset,
  auto-increment allocator, physical insert path, and statement metadata
- implement explicit MySQL-order duplicate checks and deletes rather than
  lowering directly to SQLite `INSERT OR REPLACE`
- parse `LOW_PRIORITY` as a no-op modifier and `DELAYED` as a warning-producing
  normal replace
- keep `REPLACE ... SELECT` and `REPLACE ... TABLE` runtime-deferred until
  insert-from-query source execution is specified and implemented

Out of scope for the first executable slice:

- partitions
- triggers, foreign keys, and generated-column execution
- privileges
- views
- temporary-table interactions
- binary logging, replication safety markers, and exact protocol OK-packet text
- full type conversion, range clipping, string truncation, temporal coercion,
  and SQL-mode variants beyond the existing insert slices
- `NO_AUTO_VALUE_ON_ZERO`, `auto_increment_increment`,
  `auto_increment_offset`, and non-InnoDB auto-increment variants

## Sources

- MySQL 8.4 Reference Manual, `REPLACE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, `INSERT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `INSERT ... SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, `VALUES` statement:
  https://dev.mysql.com/doc/refman/8.4/en/values.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, InnoDB `AUTO_INCREMENT` handling:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-auto-increment-handling.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` and generated columns:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-generated-columns.html
- MySQL 8.4 Reference Manual, foreign key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- MySQL 8.4 Reference Manual, trigger syntax:
  https://dev.mysql.com/doc/refman/8.4/en/trigger-syntax.html
- Existing MyLite specs:
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/insert-ignore/specs.md`
  - `docs/specs/insert-on-duplicate-key-update/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/drop-table/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849` with `mysql:8.4.9`, using focused probes through:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --table --force -vvv
```

The verified server reported version `8.4.9`, default session SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
and `@@innodb_autoinc_lock_mode = 2`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Syntax and modifiers

MySQL accepts optional `INTO`, the `VALUE` synonym, row constructors, SET form,
SELECT source, TABLE source, and `LOW_PRIORITY`:

```sql
REPLACE t VALUES (1, 10);
REPLACE INTO t VALUE (2, 20);
REPLACE INTO t VALUES ROW(3, 30), ROW(4, 40);
REPLACE INTO t SET id = 5, v = 50;
REPLACE INTO t SELECT id, v FROM src ORDER BY id;
REPLACE INTO t TABLE src;
REPLACE LOW_PRIORITY INTO t VALUES (10, 100);
```

`LOW_PRIORITY` is accepted without a warning in the verified InnoDB runtime.
`DELAYED` is accepted, converted to an ordinary replace, and records warning
3005:

```sql
REPLACE DELAYED INTO t VALUES (11, 110);
SHOW WARNINGS;
```

The warning message observed was:
`REPLACE DELAYED is no longer supported. The statement was converted to
REPLACE.`

`REPLACE HIGH_PRIORITY ...` and `REPLACE IGNORE ...` are syntax errors 1064.
There is no `REPLACE IGNORE` conflict-demotion mode. Duplicate-key conflicts
are the normal replacement trigger.

### Candidate row construction

`REPLACE` builds the row it will attempt to insert using insert-like default
rules:

- omitted columns use explicit defaults, nullable implicit `NULL`, generated
  auto-increment values, or required-column errors
- `DEFAULT` requests the target column default
- strict-mode missing required non-auto columns fail with 1364
- explicit `NULL` into a required non-auto column fails with 1048
- generated columns may be explicitly supplied only as `DEFAULT`

For `REPLACE ... SET`, assignment targets may be unqualified,
table-qualified, or schema-table-qualified in the verified runtime. The
assignment list is evaluated left to right against the new candidate row. The
old row that may later be deleted is not visible to right-hand expressions.
Columns start with their default or implicit candidate value. A same-column
reference before the assignment sees that default candidate value; later
assignments can see earlier writes.

Observed example:

```sql
CREATE TABLE set_default(
  id INT PRIMARY KEY,
  v INT DEFAULT 7,
  c INT DEFAULT 2,
  n INT
);
REPLACE INTO set_default VALUES (1,10,20,30);
REPLACE INTO set_default SET id=1, v=v+1, c=v+2, n=n+1;
```

The replacement row was `(id=1, v=8, c=10, n=NULL)`. `v=v+1` used
`DEFAULT(v)=7`; `c=v+2` saw the earlier assignment to `v=8`; `n=n+1` remained
`NULL`.

### Conflict handling

`REPLACE` first attempts to insert the candidate row. When a primary-key or
unique-key duplicate is found, MySQL deletes the conflicting stored row and
tries the insert again. It repeats this loop until the candidate can be
inserted or a fatal condition occurs.

Consequences:

- A table without any primary or unique key behaves like `INSERT`.
- A single source row can delete more than one old row when different unique
  indexes conflict with different stored rows.
- The final row is a newly inserted row; it is not an update of the old row.
- Nullable unique-key parts follow MySQL unique-index behavior: `NULL` values
  do not conflict with other `NULL` values.
- The conflict loop must be explicit in MyLite. SQLite `INSERT OR REPLACE`
  cannot be treated as compatible without preserving MySQL row construction,
  conflict order, delete count, trigger/FK behavior, diagnostics, and affected
  rows.

Observed delete-all-conflicting-rows probe:

```sql
CREATE TABLE multi_conflict(
  id INT PRIMARY KEY,
  u INT UNIQUE,
  note VARCHAR(20)
);
INSERT INTO multi_conflict
VALUES (1,10,'id conflict'),(2,20,'u conflict'),(3,30,'untouched');
REPLACE INTO multi_conflict VALUES (1,20,'new row');
```

MySQL reported affected rows `3` and left:

| id | u | note |
| --- | --- | --- |
| 1 | 20 | `new row` |
| 3 | 30 | `untouched` |

### Multi-row and query-source ordering

Source rows are processed in source order. Later source rows see rows inserted
or replaced by earlier source rows.

Observed multi-row values probe:

```sql
CREATE TABLE multi_order(id INT PRIMARY KEY, u INT UNIQUE, note VARCHAR(20));
INSERT INTO multi_order VALUES (1,100,'seed'),(2,200,'keep');
REPLACE INTO multi_order
VALUES (3,100,'first source'),(4,100,'second source');
```

MySQL reported `Records: 2 Duplicates: 2`, affected rows `4`, and left:

| id | u | note |
| --- | --- | --- |
| 2 | 200 | `keep` |
| 4 | 100 | `second source` |

`REPLACE ... SELECT` follows the selected row order. If the query result order
is not deterministic, the final replaced rows are not deterministic either.
The official documentation flags this as unsafe for statement-based
replication. Runtime probes with explicit `ORDER BY seq ASC` versus
`ORDER BY seq DESC` produced different surviving rows for duplicate source
keys, with the later source row winning.

### Affected rows and counts

Affected rows are the sum of deleted rows and inserted rows.

- A source row that inserts without deleting contributes `1`.
- A source row that deletes one old row and inserts contributes `2`.
- A source row that deletes two old rows and inserts contributes `3`.
- Multi-row statements sum the contribution of each source row.

The OK information string for multi-row `REPLACE` reports:

- `Records`: processed source rows
- `Duplicates`: old rows deleted by replacement
- `Warnings`: warning count

Observed probe:

```sql
CREATE TABLE duplicate_count_probe(
  id INT PRIMARY KEY,
  u INT UNIQUE,
  note VARCHAR(20)
);
INSERT INTO duplicate_count_probe VALUES (1,10,'one'),(2,20,'two');
REPLACE INTO duplicate_count_probe
VALUES (1,20,'replaces two'),(3,30,'new');
```

MySQL reported `Records: 2 Duplicates: 2 Warnings: 0` and affected rows `4`.

### Diagnostics and warnings

Verified MySQL 8.4.9 diagnostics for the scoped surface:

| Condition | Code | SQLSTATE | Behavior |
| --- | --- | --- | --- |
| no selected schema for unqualified target | 1046 | `3D000` | Error before mutation. |
| qualified missing schema | 1049 | `42000` | Error before mutation. |
| missing target table in selected schema | 1146 | `42S02` | Error before mutation. |
| duplicate target column in column list | 1110 | `42000` | Error before mutation. |
| unknown target column | 1054 | `42S22` | Error before mutation. |
| wrong value count | 1136 | `21S01` | Error before mutation. |
| explicit `NULL` for required non-auto column | 1048 | `23000` | Error and statement rollback. |
| omitted/defaulted required no-default column | 1364 | `HY000` | Error and statement rollback. |
| explicit non-`DEFAULT` value for generated column | 3105 | `HY000` | Error and statement rollback. |
| `REPLACE DELAYED` | 3005 | `HY000` | Warning; statement runs as ordinary `REPLACE`. |
| `REPLACE HIGH_PRIORITY` | 1064 | `42000` | Syntax error. |
| `REPLACE IGNORE` | 1064 | `42000` | Syntax error. |

Full conversion, range, truncation, invalid temporal, character-set, and
collation warning behavior follows insert semantics and remains tied to the
broader conversion roadmap.

### AUTO_INCREMENT and LAST_INSERT_ID

`REPLACE` is an InnoDB "`INSERT`-like" statement for auto-increment allocation.
For the verified default SQL mode:

- omitted, `NULL`, `0`, and `DEFAULT` auto-increment values allocate generated
  ids
- generated ids used by successful rows set session `LAST_INSERT_ID()` to the
  first generated id successfully inserted by the statement
- explicit nonzero auto-increment values do not change `LAST_INSERT_ID()`
- explicit values higher than the current counter can affect the next generated
  value
- generated candidates that replace existing rows still use the generated id
  in the newly inserted row

Observed fresh table probe:

```sql
CREATE TABLE ai_replace(
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  u INT UNIQUE,
  v INT DEFAULT 0
) AUTO_INCREMENT=10;
REPLACE INTO ai_replace(u,v) VALUES (1,100);
REPLACE INTO ai_replace(u,v) VALUES (1,200);
```

The first statement inserted id `10`, affected rows `1`, and set
`LAST_INSERT_ID()` to `10`. The second generated id `11`, deleted the old
`u=1` row, inserted `(11,1,200)`, affected rows `2`, and set
`LAST_INSERT_ID()` to `11`.

Explicit ids do not set the session value:

```sql
REPLACE INTO ai_replace(id,u,v) VALUES (5,2,500);
REPLACE INTO ai_replace(id,u,v) VALUES (5,3,600);
REPLACE INTO ai_replace(id,u,v) VALUES (20,3,700);
```

All three left `LAST_INSERT_ID()` unchanged in the verified runtime. A later
generated replacement of `u=3` inserted id `21` and set `LAST_INSERT_ID()` to
`21`.

When `NO_AUTO_VALUE_ON_ZERO` is enabled, explicit `0` is stored rather than
being treated as a generated value. The verified runtime inserted id `5` for
`REPLACE ... VALUES (0,10)` under default mode with `AUTO_INCREMENT=5`, then
stored an additional row with id `0` after enabling
`NO_AUTO_VALUE_ON_ZERO`. MyLite should defer this SQL-mode variant with the
existing insert auto-increment SQL-mode work.

If a statement fails, official MySQL documentation treats `LAST_INSERT_ID()`
after the error as undefined. The verified MySQL 8.4.9 runtime still consumed
generated ids and returned the first generated id from a row that had been
inserted before the statement rolled back. MyLite should keep the deterministic
policy already documented by the insert specs: generated ids consumed by a
statement remain consumed, and the session last insert id follows the same
first-generated-row behavior for MyLite tests even when MySQL labels the
post-error value undefined.

### Atomicity and rollback

For transactional InnoDB tables, a fatal error in any source row rolls back all
physical inserts and deletes performed by that statement.

Observed probe:

```sql
CREATE TABLE ai_atomic(
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  u INT UNIQUE,
  nn INT NOT NULL
) AUTO_INCREMENT=30;
INSERT INTO ai_atomic(u, nn) VALUES (1, 10);
REPLACE INTO ai_atomic(u, nn) VALUES (2, 20), (3, NULL);
```

The second source row failed with 1048, the first source row did not survive,
and the seed row remained. Generated ids for the failed statement were
consumed: a following generated replace inserted id `33`.

MyLite should use a statement transaction or savepoint for row changes while
preserving the existing auto-increment side-effect policy outside physical row
rollback.

### Triggers, foreign keys, and generated columns

These behaviors shape the runtime design. MyLite implements the covered
foreign-key subset; triggers and generated-column execution remain deferred.

Triggers:

- A replacing row fires insert and delete triggers, not update triggers.
- The verified trigger order for replacing an existing row was:
  `BEFORE INSERT`, `BEFORE DELETE`, `AFTER DELETE`, `AFTER INSERT`.
- A nonconflicting row fires only insert triggers.

Foreign keys:

- `ON DELETE CASCADE` fires because the old parent row is deleted. The verified
  cascade probe deleted the child row and reported affected rows `2` for the
  parent delete plus parent insert; cascaded child rows did not add to affected
  rows.
- `ON DELETE RESTRICT` can make `REPLACE` fail even when the new row has the
  same parent key value. The verified probe failed with 1451 and left parent
  and child rows unchanged.

Generated columns:

- Generated values participate in unique-key conflict detection.
- Replacing a row whose stored generated unique value conflicts with the
  candidate deletes the old row and inserts the new candidate row.
- Explicit generated-column values are accepted only as `DEFAULT`; a nondefault
  explicit value failed with 3105 in the verified runtime.

Until these features exist in MyLite, `REPLACE` must not silently claim their
effects. Parser acceptance should either reject such runtime use
deterministically or leave the compatibility row partial with explicit gaps.

## MyLite behavior

### Parser and AST

Add a top-level `replace_statement` AST node rather than reusing an insert
statement node. `REPLACE` shares insert source machinery, but it has different
modifiers, no `IGNORE`, no row aliases, no ODKU clause, and delete-plus-insert
side effects.

The statement node should record:

1. target table name
2. source kind: `VALUES`, `SET`, `SELECT`, or `TABLE`
3. optional column list, including an explicit empty `()`
4. row list, assignment list, query expression, or source table name
5. modifier flags for `LOW_PRIORITY` and `DELAYED`

Column lists and value rows should reuse the existing insert AST node shapes
where practical. `SET` assignment targets should reuse qualified identifier
handling from `INSERT ... SET`, because MySQL accepts qualified targets for
`REPLACE ... SET` in runtime probes. The assignment list must preserve source
order.

`REPLACE ... SELECT` and `REPLACE ... TABLE` need AST representation even if
runtime remains unsupported in the first implementation. Once query-source
inserts exist, the runtime should consume rows from the query in result order.

The parser must not accept `IGNORE`, `HIGH_PRIORITY`, row aliases,
`ON DUPLICATE KEY UPDATE`, or partition clauses for the first slice unless
their own specs have been implemented.

### Lemon grammar snippets

These snippets describe MyLite's intended `REPLACE` grammar and are
independently authored for MyLite's parser:

```lemon
replace_statement ::= REPLACE opt_replace_modifier opt_into table_name
                      opt_insert_column_list replace_values_keyword
                      replace_row_list.

replace_statement ::= REPLACE opt_replace_modifier opt_into table_name
                      opt_insert_column_list VALUES replace_row_constructor_list.

replace_statement ::= REPLACE opt_replace_modifier opt_into table_name SET
                      replace_assignment_list.

replace_statement ::= REPLACE opt_replace_modifier opt_into table_name
                      opt_insert_column_list query_expression.

replace_statement ::= REPLACE opt_replace_modifier opt_into table_name
                      opt_insert_column_list TABLE table_name.

opt_replace_modifier ::= .
opt_replace_modifier ::= LOW_PRIORITY.
opt_replace_modifier ::= DELAYED.

opt_into ::= .
opt_into ::= INTO.

opt_insert_column_list ::= .
opt_insert_column_list ::= LPAREN RPAREN.
opt_insert_column_list ::= LPAREN insert_column_list RPAREN.

insert_column_list ::= identifier.
insert_column_list ::= insert_column_list COMMA identifier.

replace_values_keyword ::= VALUES.
replace_values_keyword ::= VALUE.

replace_row_list ::= replace_row.
replace_row_list ::= replace_row_list COMMA replace_row.

replace_row ::= LPAREN opt_replace_value_list RPAREN.

replace_row_constructor_list ::= ROW LPAREN opt_replace_value_list RPAREN.
replace_row_constructor_list ::= replace_row_constructor_list COMMA ROW
                                 LPAREN opt_replace_value_list RPAREN.

opt_replace_value_list ::= .
opt_replace_value_list ::= replace_value_list.

replace_value_list ::= replace_value.
replace_value_list ::= replace_value_list COMMA replace_value.

replace_value ::= expression.
replace_value ::= DEFAULT.

replace_assignment_list ::= replace_assignment.
replace_assignment_list ::= replace_assignment_list COMMA replace_assignment.

replace_assignment ::= qualified_identifier EQ replace_value.
```

The following MySQL-valid surface remains deliberately outside the first
implementation slice:

```lemon
/* Deferred: partition routing. */
replace_statement ::= REPLACE opt_replace_modifier opt_into table_name
                      replace_partition_clause opt_insert_column_list
                      replace_values_keyword replace_row_list.

/* Invalid for MySQL REPLACE and must stay rejected. */
replace_statement ::= REPLACE IGNORE opt_into table_name replace_values_keyword
                      replace_row_list.
replace_statement ::= REPLACE HIGH_PRIORITY opt_into table_name
                      replace_values_keyword replace_row_list.

/* Not part of REPLACE syntax. */
replace_statement ::= REPLACE opt_replace_modifier opt_into table_name
                      opt_insert_column_list replace_values_keyword
                      replace_row_list insert_row_alias
                      insert_duplicate_key_update_clause.
```

### Analyzer and execution model

Execution should use a custom statement handle. The first `mylite_step()`
performs validation and side effects; later steps return `MYLITE_DONE`.

High-level runtime flow:

1. Resolve target schema and table using the same rules as insert.
2. Reject system schemas and statement-level structural errors before mutation.
3. Resolve the optional column list or SET assignment targets.
4. Build source rows in source order using insert-compatible candidate-row
   construction.
5. For each source row, allocate auto-increment values as insert would.
6. Validate required columns, defaults, generated-column boundaries, and value
   conversion for the supported column types.
7. Try to insert the candidate row.
8. If a primary or unique conflict is detected, delete the conflicting stored
   row and retry the same candidate.
9. Repeat conflict deletion until no conflict remains or a fatal condition
   occurs.
10. Commit statement row changes on success, or roll them back on fatal error.

Duplicate detection must use MyLite's catalog primary/unique index ordering
rather than SQLite's arbitrary constraint reporting. It must consider:

- rows that existed before the statement
- rows inserted by earlier source rows
- rows that remain after earlier source rows deleted conflicting rows
- nullable unique-key parts, where `NULL` does not conflict

The conflict loop must delete every conflicting stored row needed to make the
candidate insert succeed. A single source row therefore can affect more than
two rows.

### Candidate-row construction

`REPLACE ... VALUES` should reuse the `INSERT ... VALUES` candidate builder:

- omitted column list maps rows to all table columns in ordinal order
- explicit column list maps source values to those columns
- explicit empty column list plus empty row builds an all-default row
- `VALUE` and `VALUES ROW(...)` are synonyms for the scoped candidate builder
- `DEFAULT`, `NULL`, deterministic literals, supported expression values, and
  `CURRENT_TIMESTAMP` follow existing insert support

`REPLACE ... SET` should reuse `INSERT ... SET` assignment-order mechanics with
one important semantic framing: expressions read the candidate row, not the old
stored row that may be deleted later. Candidate slots are initialized from
defaults and implicit values. Assignments then run left to right, and earlier
assignments are visible to later assignments.

`REPLACE ... SELECT` and `REPLACE ... TABLE` should eventually use the
insert-from-query candidate builder. The query result's row order determines
replacement order. If the query has no deterministic order, MyLite should
mirror MySQL by making no stronger guarantee.

### Metadata and public API

`REPLACE` produces no result set. It must update statement and session
metadata:

- `mylite_affected_rows(stmt)` returns inserted rows plus deleted rows
- statement state tracks processed source records
- statement state tracks duplicate count as deleted old rows
- statement state tracks warnings, including 3005 for `DELAYED`
- `mylite_last_insert_id(db)` changes to the first generated auto-increment id
  from a successfully inserted source row, using the same deterministic MyLite
  policy as current inserts for statement rollback cases

The eventual protocol layer should format `Records`, `Duplicates`, and
`Warnings` from the same statement state.

### Storage and transaction handling

Use a statement savepoint or transaction around physical row changes:

- accepted source rows survive successful completion
- rows deleted by successful replacements stay deleted
- rows inserted by successful replacements stay inserted
- fatal errors roll back all physical deletes and inserts performed by the
  statement

Auto-increment metadata must be handled separately from physical row rollback,
following the existing insert policy:

- generated values consumed before a fatal error remain consumed where MySQL's
  InnoDB behavior consumes them
- generated values for replaced rows are not reused
- explicit high values advance the next generated value according to the
  existing allocator rules

Avoid SQLite `INSERT OR REPLACE` as the main semantic mechanism. It does not
provide enough control over MySQL candidate validation, repeated conflict
deletion, affected rows, duplicate counts, trigger/FK ordering, or diagnostics.

### SQL modes

The first implementation should target the default MySQL 8.4.9 strict mode
used by compatibility tests. It should share SQL-mode hooks with insert so
future mode work can add:

- `NO_AUTO_VALUE_ON_ZERO`
- relaxed strict-mode default handling
- conversion warning versus error decisions
- zero-date and invalid-temporal handling

`IGNORE` is not a SQL mode interaction for `REPLACE`; it is invalid syntax.

### Explicit deferred behavior

MyLite intentionally documents these first-slice boundaries:

- `REPLACE ... SELECT` and `REPLACE ... TABLE` runtime wait for
  insert-from-query support.
- Partition clauses wait for partition metadata and routing.
- `DELAYED` warning 3005 is implemented for the executable `VALUES` and `SET`
  sources.
- Trigger execution waits for trigger DDL and runtime support.
- Foreign-key `ON DELETE CASCADE`, `ON DELETE SET NULL`, `RESTRICT`, and
  `NO ACTION` behavior is implemented for supported conflict deletes.
- Generated-column conflict detection and explicit generated-column validation
  wait for generated-column execution.
- Full conversion, range, truncation, temporal, charset, and collation fidelity
  wait for the shared conversion roadmap.
- Privileges are out of scope until MyLite has an authorization model.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `REPLACE t VALUES (1,10)` | Accepted without `INTO`; affected rows `1`. |
| `REPLACE INTO t VALUE (2,20)` | Accepted; `VALUE` is a synonym. |
| `REPLACE INTO t VALUES ROW(3,30), ROW(4,40)` | Inserts both rows; affected rows `2`. |
| `REPLACE INTO t SET id=5, v=50` | SET form accepted; affected rows `1` when no conflict. |
| `REPLACE INTO t SELECT ... ORDER BY ...` | Processes query rows in result order once query-source runtime exists. |
| `REPLACE INTO t TABLE src` | Processes TABLE source rows once table-source runtime exists. |
| `REPLACE LOW_PRIORITY INTO t VALUES (...)` | Accepted; no warning in verified InnoDB runtime. |
| `REPLACE DELAYED INTO t VALUES (...)` | Executes as normal replace and records warning 3005. |
| `REPLACE HIGH_PRIORITY INTO t VALUES (...)` | Syntax error 1064. |
| `REPLACE IGNORE INTO t VALUES (...)` | Syntax error 1064. |
| no selected schema | Error 1046; no mutation. |
| qualified missing schema | Error 1049; no mutation. |
| missing target table | Error 1146; no mutation. |
| duplicate target column list | Error 1110; no mutation. |
| unknown target column | Error 1054; no mutation. |
| wrong row arity | Error 1136; no mutation. |
| explicit `NULL` into required non-auto column | Error 1048; no mutation. |
| required non-auto column omitted or set to `DEFAULT` with no default | Error 1364; no mutation. |
| table with no primary/unique key | Behaves like insert; affected rows are inserted rows only. |
| single primary-key conflict | Deletes old row, inserts candidate, affected rows `2`. |
| single source row conflicts with two different old rows through two unique indexes | Deletes both old rows, inserts candidate, affected rows `3`. |
| multi-row statement where later source row conflicts with earlier inserted source row | Later row deletes earlier row; final table reflects source order. |
| multi-row values with two processed rows and two deleted old rows | OK counts record `Records: 2 Duplicates: 2 Warnings: 0`; affected rows `4`. |
| `REPLACE ... SELECT` with ascending versus descending source order | Final surviving duplicate-key row follows source order. |
| `REPLACE ... SET id=1, v=v+1, c=v+2` over `v DEFAULT 7` | Stores `v=8`; later assignment reads earlier candidate write. |
| generated auto id with no conflict | Inserts generated id, affected rows `1`, last insert id set to generated id. |
| generated auto id with unique conflict | Deletes old row, inserts new generated id, affected rows `2`, last insert id set to generated id. |
| explicit nonzero auto id insert or replacement | Does not change last insert id. |
| `NO_AUTO_VALUE_ON_ZERO` disabled; explicit `0` in auto column | Generates next id and sets last insert id. |
| `NO_AUTO_VALUE_ON_ZERO` enabled; explicit `0` in auto column | Stores id `0`; does not set last insert id. Deferred until SQL-mode work. |
| fatal error in later source row after earlier row mutation | Rolls back all statement row changes; consumed generated ids remain consumed according to MyLite insert policy. |
| replacing parent row with `ON DELETE CASCADE` child | Child is cascaded; parent replace affected rows `2`. |
| replacing parent row with `ON DELETE SET NULL` child | Child FK columns become `NULL`; parent replace affected rows `2`. |
| replacing parent row with `ON DELETE RESTRICT` child | Error 1451; parent and child unchanged. |
| triggers on replacing row | Fires `BEFORE INSERT`, `BEFORE DELETE`, `AFTER DELETE`, `AFTER INSERT`; no update triggers. Deferred. |
| generated unique column conflict | Generated value can be the conflict key. Deferred. |
| explicit generated column value other than `DEFAULT` | Error 3105. Deferred. |

## Test plan

Parser tests:

- optional `INTO`
- `VALUE` synonym
- ordinary multi-row values
- `VALUES ROW(...)` constructor list
- explicit column lists, empty column lists, all-default rows, and empty rows
- `SET` assignment list with qualified and unqualified targets
- `SELECT` and `TABLE` source forms when the query grammar can represent them
- `LOW_PRIORITY` and `DELAYED`
- parse rejection for `HIGH_PRIORITY`, `IGNORE`, row aliases, ODKU clauses,
  malformed assignment lists, malformed row lists, and deferred partition
  clauses

Runtime tests for first executable slice:

- target schema resolution through default schema and qualified schema
- missing schema/table/system-schema diagnostics
- duplicate/unknown target columns and wrong arity
- successful insert-equivalent replace into a table with no unique key
- primary-key conflict replacement
- unique-key conflict replacement
- one source row deleting multiple old rows through multiple unique indexes
- multi-row source order, including later rows replacing earlier inserted rows
- affected rows, processed records, duplicate count, warning count, and last
  insert id
- `DELAYED` warning 3005
- SET assignment-order behavior and lack of access to deleted-row values
- auto-increment omitted, `NULL`, `0`, `DEFAULT`, explicit low, explicit high,
  duplicate replacement, sequence consumption, and session last insert id
- fatal error rollback after earlier inserted/deleted rows
- deterministic unsupported diagnostics for deferred trigger/generated surfaces

Deferred runtime tests:

- `REPLACE ... SELECT` and `REPLACE ... TABLE` source ordering and metadata
- partition routing and partition mismatch diagnostics
- trigger order and trigger side effects
- generated-column default and conflict behavior
- type conversion, range clipping, truncation, invalid temporal values, and
  SQL-mode warning/error variants
- protocol OK-packet information string formatting

MySQL-runtime comparison tests must verify result rows, warning codes/messages,
warning ordering, affected rows, processed record count, duplicate count, last
insert id, auto-increment side effects, and absence of mutation for fatal
errors.

## Implementation risks

- SQLite `INSERT OR REPLACE` performs a superficially similar operation but
  does not preserve MySQL's observable semantics by itself.
- The conflict loop can delete multiple rows for one source row; stopping after
  the first duplicate would be wrong.
- Source-order behavior matters for multi-row `VALUES` and query sources.
- SET expressions must use the candidate row, not the deleted row.
- Auto-increment side effects must remain separate from row rollback.
- Affected rows count deletes plus inserts, while duplicate count tracks old
  rows deleted by replacement.
- Trigger and foreign-key behavior proves the operation is delete plus insert
  for user-visible semantics; later support must not implement it as a silent
  update.
