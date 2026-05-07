# INSERT ... ON DUPLICATE KEY UPDATE

## Scope

This feature specifies MySQL-compatible `INSERT ... ON DUPLICATE KEY UPDATE`
behavior for the insert forms MyLite already supports or has already
specified:

- `INSERT [IGNORE] [INTO] table_name [(column_list)] VALUES ...`
- `INSERT [IGNORE] [INTO] table_name [(column_list)] VALUE ...`
- `INSERT [IGNORE] [INTO] table_name [(column_list)] VALUES ROW(...) ...`
- `INSERT [IGNORE] [INTO] table_name SET assignment_list`

Current implementation status:

- Implemented for the currently supported `INSERT ... VALUES`, `VALUE`,
  `VALUES ROW(...)`, `INSERT ... SET`, and first `INSERT IGNORE` surfaces.
- The implemented slice includes row aliases, optional column aliases,
  catalog-order duplicate conflict selection, source-order update assignments,
  repeated update targets, `DEFAULT`, target-row references, candidate-row
  references through row aliases/column aliases/`VALUES(col)`, warning 1287 for
  each syntactic `VALUES(col)` use, update-branch duplicate rollback, and
  `INSERT IGNORE` demotion/continuation for update-branch duplicate conflicts.
- Generated columns, triggers, foreign keys, insert-from-query sources,
  partitions, priority/delayed modifiers, explicit `LAST_INSERT_ID(expr)`, full
  conversion/range/truncation demotion, and broad expression support remain
  deferred.

In scope:

- conflict detection against primary and unique keys for candidate insert rows
- updating the conflicting existing row instead of inserting the candidate row
- multi-row execution where each source row independently inserts, updates,
  no-ops, errors, or is ignored under `IGNORE`
- source-order evaluation of duplicate-key update assignments
- duplicate-key update assignments to target columns, repeated assignment
  targets, `DEFAULT`, column references, and candidate-row references
- `VALUES(col_name)` in the duplicate-key update clause, including MySQL 8.4.9
  deprecation warnings
- row aliases and optional column aliases after `VALUES`, `VALUE`, `ROW(...)`,
  and `SET` insert sources
- affected rows, duplicate counts, warning counts, session last insert id, and
  `AUTO_INCREMENT` side effects for the scoped insert forms
- error handling when the duplicate-key update itself violates a primary or
  unique key
- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE` demotion for duplicate errors
  raised by the update branch
- MyLite Lemon-style grammar snippets for the intended grammar

Out of scope:

- `INSERT ... SELECT`, `INSERT ... TABLE`, and standalone `VALUES`
- partition routing and partition mismatch diagnostics
- `LOW_PRIORITY`, `HIGH_PRIORITY`, and deprecated `DELAYED`
- generated-column runtime support beyond documenting MySQL behavior
- triggers, foreign keys, views, privileges, binary logging, replication safety
  flags, and storage-engine variants
- full type conversion, range clipping, string truncation, temporal coercion,
  and SQL-mode variants beyond the existing insert and `INSERT IGNORE` slices
- `CLIENT_FOUND_ROWS` public API behavior unless MyLite exposes a connection
  flag equivalent in the same implementation
- exact MySQL protocol OK-packet text until the wire-protocol layer owns it

## Sources

- MySQL 8.4 Reference Manual, `INSERT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `INSERT ... ON DUPLICATE KEY UPDATE`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- Existing MyLite specs:
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/insert-ignore/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/column-attributes/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849` with `mysql:8.4.9`, using focused probes through:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --table --force -vvv
docker exec -i mylite-mysql-849 mysql -uroot --table --force --show-warnings -vvv
```

The verified server reported version `8.4.9`, default session SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
and `@@innodb_autoinc_lock_mode = 2`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Syntax

`ON DUPLICATE KEY UPDATE` follows the insert source, after any row alias:

```sql
INSERT INTO t(a, b) VALUES (1, 2)
ON DUPLICATE KEY UPDATE b = VALUES(b);

INSERT INTO t SET a = 1, b = 2
ON DUPLICATE KEY UPDATE b = VALUES(b);

INSERT INTO t(a, b) VALUES ROW(1, 2), ROW(3, 4)
AS new_row
ON DUPLICATE KEY UPDATE b = new_row.b;
```

The duplicate-key update list is mandatory after `ON DUPLICATE KEY UPDATE`.
Each assignment uses a target column and a value expression. MySQL permits
repeated targets in the update list; assignments are applied in source order,
so later assignments see earlier assignment effects.

`VALUES(col_name)` is accepted only in the duplicate-key update context and
returns the value that the candidate insert row would have contributed for that
column. In MySQL 8.4.9, each `VALUES()` use in an ODKU statement records
warning 1287 recommending row aliases instead. A statement with three
`VALUES()` calls produced three 1287 warnings in the runtime probes.

Row aliases and optional column aliases are accepted after `VALUES`, `VALUE`,
`VALUES ROW(...)`, and `SET` forms. Column aliases require a row alias. The row
alias must not equal the target table name. Column aliases for the inserted row
must be unique. If column aliases shadow target table column names, unqualified
references to those names in the duplicate-key update list are ambiguous.

Observed alias diagnostics:

| Statement shape | Diagnostic |
| --- | --- |
| row alias same as target table | 1066 / `42000`, not unique table or alias |
| column alias count does not match insert source width | 1353 / `HY000`, column-count mismatch |
| duplicate column alias | 1060 / `42S21`, duplicate column name |
| unqualified name matching both target column and column alias | 1052 / `23000`, ambiguous column |

### Candidate row validation before update

MySQL first builds and validates the candidate insert row. The duplicate-key
update branch is not a shortcut around insert-side validation. For example:

- omitting a required non-auto column without a default fails with 1364 before
  the update list executes, even if the duplicate-key branch would not read or
  write that column
- explicitly assigning a stored generated column in the insert source fails
  with 3105 before the update list executes
- wrong row arity, unknown insert-list columns, duplicate insert-list columns,
  and missing target tables remain statement errors

This ordering matters because the implementation cannot resolve only the
conflict key and skip ordinary insert validation.

### Conflict selection

If the candidate row does not conflict with any primary or unique key, MySQL
inserts it normally. If it conflicts, MySQL updates one existing row.

When a candidate row conflicts with multiple unique indexes that point to
different rows, MySQL updates only one row. Runtime probes showed the selected
row follows unique-index order:

| Unique indexes | Candidate | Updated row |
| --- | --- | --- |
| `UNIQUE KEY ua(a), UNIQUE KEY ub(b)` | `(a=1, b=20)` | row with `a=1` |
| `UNIQUE KEY ub(b), UNIQUE KEY ua(a)` | `(a=1, b=20)` | row with `b=20` |

The primary key participates in the same conflict surface. MyLite should use
its catalog index ordering for deterministic MySQL-compatible conflict
selection and should document any remaining gap if SQLite reports a different
constraint first.

The official MySQL documentation warns against relying on ODKU with multiple
unique indexes. MyLite must still preserve MySQL-compatible behavior for tests
because common applications can accidentally depend on it.

### Duplicate-key update assignment evaluation

The duplicate-key update branch behaves like a single-row `UPDATE` over the
conflicting row:

- update assignments are evaluated and applied left to right
- ordinary target column references read the current existing row value at the
  time each assignment is evaluated
- earlier assignments are visible to later assignments
- candidate-row values are read through `VALUES(col)`, row aliases, or column
  aliases
- repeated assignment targets are allowed; the last effective assignment wins,
  with earlier assignments still visible to later expressions
- assigning a column to its existing value makes that column a no-op

Observed source-order examples:

```sql
CREATE TABLE order_probe(id INT PRIMARY KEY, a INT DEFAULT 3,
                         b INT DEFAULT 4, c INT DEFAULT 5);
INSERT INTO order_probe VALUES (1, 10, 20, 30);

INSERT INTO order_probe VALUES (1, 40, 50, 60)
ON DUPLICATE KEY UPDATE b = a + 1, a = VALUES(a), c = b + 1;
```

The stored row became `(a=40, b=11, c=12)`: `b` read the old `a=10`, `a` then
used the candidate value `40`, and `c` read the newly updated `b=11`.

For repeated targets:

```sql
INSERT INTO order_probe VALUES (1, 1, 2, 3)
ON DUPLICATE KEY UPDATE a = 100, a = a + 1, b = a + 1;
```

The stored row became `(a=101, b=102)`.

### Affected rows, duplicate counts, and no-op updates

Per candidate row, MySQL reports affected rows as:

- `1` when the row is inserted
- `2` when an existing row is updated and at least one stored value changes
- `0` when the duplicate-key update sets the row to its current values

The client `CLIENT_FOUND_ROWS` flag changes the no-op update count from `0` to
`1`. MyLite should keep the default behavior until it exposes a compatible
connection flag.

For multi-row statements, affected rows are the sum across candidate rows. The
OK information string records processed records, duplicates, and warnings; the
implementation should track those counts even if the first public API exposes
only affected rows and warnings.

Observed examples:

| Statement effect | MySQL result |
| --- | --- |
| insert via ODKU statement | `Query OK, 1 row affected` |
| duplicate branch changes values | `Query OK, 2 rows affected` |
| duplicate branch no-op | `Query OK, 0 rows affected` |
| three candidates: two duplicate updates and one insert | `Query OK, 5 rows affected`, `Records: 3 Duplicates: 2` |

### Warnings

`VALUES(col)` in an ODKU update expression records warning 1287 in MySQL
8.4.9 once per syntactic use in the statement, even when every candidate row
takes the insert path. Row-alias and column-alias forms avoid that warning.

`SHOW WARNINGS` and `SHOW COUNT(*) WARNINGS` expose warnings from the most
recent nondiagnostic statement. The `mysql` client can also print warnings
automatically with `--show-warnings`; runtime probes avoided relying on
`ROW_COUNT()` after automatic warning display because diagnostic reads can
change what later `ROW_COUNT()` observes.

When the duplicate-key update branch creates a duplicate-key conflict:

- without `IGNORE`, the statement fails with 1062 and no candidate rows from
  the statement survive
- with `IGNORE`, the update-branch duplicate error is demoted to warning 1062,
  execution continues with later source rows, and successfully inserted rows
  remain

In a two-row probe, `INSERT IGNORE ... ON DUPLICATE KEY UPDATE` where the first
row's update would duplicate another unique key and the second row inserted
successfully reported one affected row, duplicate count one, and warnings for
both the `VALUES()` deprecation and the demoted duplicate.

### Automatic `ON UPDATE` columns

When the duplicate-key branch changes an existing row, supported
`ON UPDATE CURRENT_TIMESTAMP` columns are refreshed with the statement-stable
current timestamp unless the update assignment list explicitly assigns that
column. A duplicate branch that makes no row change leaves automatic-update
columns unchanged and reports zero affected rows.

### AUTO_INCREMENT and `LAST_INSERT_ID()`

For an ODKU statement over an `AUTO_INCREMENT` table:

- generated candidate rows consume auto-increment values before conflict
  handling
- duplicate-key updates caused by generated candidate rows still consume those
  generated values
- explicit nonzero candidate ids do not advance the sequence unless they are
  accepted as ordinary explicit high inserts
- inserting a new generated row sets session `LAST_INSERT_ID()` to the first
  generated id from an inserted row
- a duplicate-key update does not set `LAST_INSERT_ID()` in the verified MySQL
  8.4.9 runtime unless the update expression explicitly calls
  `LAST_INSERT_ID(expr)`
- explicit nonzero auto-increment candidate values leave the session value
  unchanged

Observed fresh-session probe:

```sql
CREATE TABLE li_fresh(
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  u INT UNIQUE,
  v INT DEFAULT 0
) ENGINE=InnoDB AUTO_INCREMENT=30;

INSERT INTO li_fresh(id,u,v) VALUES (5,1,10);
INSERT INTO li_fresh(u,v) VALUES (1,11)
ON DUPLICATE KEY UPDATE v=VALUES(v);
```

The duplicate-key update changed the existing row, consumed generated id `30`,
advanced the next auto-increment value to `31`, and left
`LAST_INSERT_ID()` at `0`. A following generated insert stored id `31` and set
`LAST_INSERT_ID()` to `31`.

The common MySQL idiom:

```sql
ON DUPLICATE KEY UPDATE id = LAST_INSERT_ID(id)
```

sets the session value to the existing row id through normal
`LAST_INSERT_ID(expr)` function semantics. MyLite should support this when the
information-function expression surface is available to ODKU assignments.

### Defaults and `VALUES(col)`

The candidate row contains defaulted values before conflict handling. When
`VALUES(col)` reads a column that used an explicit or omitted default, it sees
the value that would have been inserted.

Observed probe:

```sql
CREATE TABLE defaults_req(
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  u INT UNIQUE,
  req INT NOT NULL,
  d INT NOT NULL DEFAULT 7,
  n INT NULL
);
INSERT INTO defaults_req(u,req) VALUES (1,10);
INSERT INTO defaults_req(u,req,d,n) VALUES (1,20,DEFAULT,5)
ON DUPLICATE KEY UPDATE req=VALUES(req), d=VALUES(d), n=VALUES(n);
```

The existing row became `req=20`, `d=7`, and `n=5`.

### Generated columns

MySQL recalculates generated columns when the duplicate-key update changes base
columns. A stored generated unique column can itself be the duplicate key that
routes a candidate row to the update branch.

Observed behavior:

- updating base column `b` changed stored generated column `g = b + 1`
- a later candidate whose generated `g` conflicted with the updated row took
  the duplicate-key branch; its update expression no-oped and affected rows
  were `0`
- explicitly supplying a value for a generated column in the insert source
  failed with 3105 before duplicate-key update execution

Generated-column execution is outside the first MyLite ODKU implementation
because MyLite generated columns are still deferred, but the implementation
must leave a deterministic unsupported diagnostic rather than silently storing
wrong generated values.

## MyLite behavior

### Parser and AST

Extend the existing `INSERT ... VALUES` and `INSERT ... SET` statement nodes
with an optional duplicate-key update clause. The clause should contain:

1. ordered duplicate-key update assignments
2. optional row alias
3. optional column aliases attached to that row alias

Do not add a separate top-level statement kind unless the parser architecture
requires it. ODKU is a modifier on insert execution, not a separate statement
family.

Each duplicate-key update assignment stores:

1. the assignment target column identifier or qualified identifier
2. the assigned expression or `DEFAULT`

The update assignment list must preserve source order. Runtime semantics depend
on that order.

The insert source continues to own candidate-row construction. The ODKU clause
must read candidate-row values through the source's resolved column mapping,
including omitted/defaulted columns.

### Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature and are
independently authored for MyLite's parser:

```lemon
insert_values_statement ::= INSERT opt_insert_ignore opt_into table_name
                            opt_insert_column_list insert_values_keyword
                            insert_row_list opt_insert_row_alias
                            opt_insert_duplicate_update.

insert_set_statement ::= INSERT opt_insert_ignore opt_into table_name SET
                         insert_set_assignment_list opt_insert_row_alias
                         opt_insert_duplicate_update.

opt_insert_duplicate_update ::= .
opt_insert_duplicate_update ::= ON DUPLICATE KEY UPDATE
                                insert_update_assignment_list.

insert_update_assignment_list ::= insert_update_assignment.
insert_update_assignment_list ::= insert_update_assignment_list COMMA
                                  insert_update_assignment.

insert_update_assignment ::= insert_update_target EQ insert_update_value.

insert_update_target ::= qualified_identifier.

insert_update_value ::= expression.
insert_update_value ::= DEFAULT.

opt_insert_row_alias ::= .
opt_insert_row_alias ::= AS identifier.
opt_insert_row_alias ::= AS identifier LPAREN insert_alias_column_list RPAREN.

insert_alias_column_list ::= identifier.
insert_alias_column_list ::= insert_alias_column_list COMMA identifier.

insert_value_function ::= VALUES LPAREN identifier RPAREN.
```

`insert_value_function` should be accepted only where the expression grammar can
recognize function-like expressions and the analyzer can verify it appears in
an ODKU expression context. Outside that context, MySQL returns `NULL` for
`VALUES()` where it is syntactically accepted as a function call; MyLite may
defer the broader compatibility surface until general function evaluation owns
it.

The existing `insert_values_keyword`, `insert_row_list`,
`opt_insert_column_list`, `insert_set_assignment_list`, and
`opt_insert_ignore` productions remain as defined by the earlier insert specs.

The following MySQL-valid forms remain deliberately outside this task:

```lemon
/* Deferred: priority and deprecated delayed modifiers. */
insert_values_statement ::= INSERT insert_priority opt_insert_ignore opt_into
                            table_name opt_insert_column_list
                            insert_values_keyword insert_row_list
                            opt_insert_row_alias
                            opt_insert_duplicate_update.

/* Deferred: partition routing. */
insert_values_statement ::= INSERT opt_insert_ignore opt_into table_name
                            insert_partition_clause opt_insert_column_list
                            insert_values_keyword insert_row_list
                            opt_insert_row_alias
                            opt_insert_duplicate_update.

/* Deferred: insert-from-query sources. */
insert_select_statement ::= INSERT opt_insert_ignore opt_into table_name
                            opt_insert_column_list query_expression
                            opt_insert_duplicate_update.
```

### Name resolution

ODKU uses two row scopes:

- the target table row being updated
- the candidate insert row

Assignment targets resolve only against target-table columns. Unqualified
ordinary column references in assignment expressions resolve against the target
table unless a column alias makes the name ambiguous. Row-alias-qualified
references resolve against candidate insert values. Column aliases resolve to
candidate insert values and can be used unqualified only when they do not
conflict with target-table column names or other visible names.
For `INSERT ... SET`, column aliases bind to the SET assignments in source
order rather than target-table ordinal order.

`VALUES(col)` resolves `col` against the target table's insertable column set,
not against arbitrary expression aliases. It returns the candidate-row value
for that column after insert-side defaults and assignment-order rules have been
applied.

Validation order:

1. Resolve the target table and insert source as the existing insert specs do.
2. Resolve and validate row aliases and column aliases.
3. Resolve ODKU assignment targets and expression names.
4. Execute candidate rows in source order.

Unknown ODKU assignment targets fail with 1054. Repeated ODKU assignment
targets are allowed and must not reuse the insert column-list duplicate-target
diagnostic.

### Execution model

Execution should reuse the existing insert pipeline until the point where a
candidate row would be physically inserted:

1. Build the candidate insert row using `INSERT ... VALUES` or `INSERT ... SET`
   semantics.
2. Perform insert-side validation, default resolution, type conversion, and
   auto-increment allocation.
3. Detect the first conflicting primary or unique key according to MySQL index
   order.
4. If no conflict exists, insert the row.
5. If a conflict exists and no ODKU clause exists, use existing insert or
   `INSERT IGNORE` behavior.
6. If a conflict exists and ODKU exists, evaluate the update list against the
   conflicting existing row and candidate row.
7. Validate the updated row against primary, unique, required-column, generated
   column, and conversion rules.
8. Apply the update, no-op, demote under `IGNORE`, or fail atomically.

The duplicate-key update branch should share the single-table `UPDATE`
assignment-order and changed-row detection helpers where possible. It should
not lower directly to SQLite's `ON CONFLICT DO UPDATE` unless MyLite can still
preserve MySQL's conflict selection, assignment order, warning order,
affected-row accounting, and diagnostics.

### Conflict detection

Duplicate detection must run against:

- rows already stored before the statement
- rows inserted or updated earlier in the same statement
- all primary and unique indexes supported by MyLite metadata

For each candidate row, scan unique constraints in MySQL-compatible catalog
order and select the first constraint whose key values match an existing row.
Nullable unique-key parts follow MySQL behavior: `NULL` values are distinct and
do not conflict with other `NULL` values.

If the selected unique constraint points to an existing row, that row is the
only row updated. If another unique constraint would point to a different row,
that does not change target selection. The later update validation may still
fail if the updated row would duplicate another row.

### Assignment evaluation

Initialize update evaluation from the existing conflicting row. For each ODKU
assignment in source order:

- evaluate the right-hand side against the current target-row image and the
  candidate-row image
- `DEFAULT` assigns the target column default, with required-column diagnostics
  matching single-table `UPDATE`
- write the result into the target-row image
- make the written value visible to later assignments

After all assignments, compare the final target-row image to the original row
using MySQL-compatible value equality for the supported column types:

- changed row: affected rows contribution `2`
- no-op row: affected rows contribution `0` by default

### `INSERT IGNORE` interaction

`INSERT IGNORE ... ON DUPLICATE KEY UPDATE` does not skip the original
duplicate row. It runs the update branch. `IGNORE` applies to ignorable errors
that occur while building the candidate row or applying the update branch.

For the first ODKU implementation, MyLite should support at least:

- demoting duplicate-key errors caused by the update branch to warning 1062
- continuing with later candidate rows after such a demoted update error
- preserving inserts and updates from other source rows in the statement
- retaining generated auto-increment consumption for the failed update
  candidate
- demoting covered numeric, string-length, and temporal conversion problems in
  update assignments through the same warning and coerced-value paths used by
  insert/update validation, with diagnostics using the source candidate row
  number

If a broader `INSERT IGNORE` conversion slice has not landed, conversion,
range, truncation, and temporal demotion outside the covered assignment paths
remain partial and must be documented in `COMPATIBILITY.md`.

### Diagnostics and warnings

ODKU needs the existing MyLite diagnostic catalog plus these additional warning
and error requirements:

| Condition | Code | SQLSTATE | Behavior |
| --- | --- | --- | --- |
| `VALUES(col)` in ODKU expression | 1287 | `HY000` | Statement succeeds and records a warning per use. |
| row alias equals target table alias/name | 1066 | `42000` | Error before mutation. |
| column alias count does not match insert source width | 1353 | `HY000` | Error before mutation. |
| duplicate column alias | 1060 | `42S21` | Error before mutation. |
| ambiguous unqualified reference | 1052 | `23000` | Error before mutation. |
| unknown ODKU assignment target or expression column | 1054 | `42S22` | Error before mutation. |
| update branch duplicates another unique key | 1062 | `23000` | Error and statement rollback; warning and continuation under `IGNORE`. |
| missing required candidate value | 1364 | `HY000` | Error before update branch unless demoted by `IGNORE`. |
| explicit generated-column insert value | 3105 | `HY000` | Error before update branch. |

Warning records must be ordered the way MySQL observes the conditions during
row processing. For example, an `INSERT IGNORE` ODKU statement with
`VALUES()` and an update-branch duplicate warning reported the 1287 warning
before the 1062 warning in the verified probe.

### Metadata and public API

ODKU produces no result set. It must update statement and session metadata:

- `mylite_affected_rows(stmt)` returns the summed affected-row count
- `mylite_last_insert_id(db)` changes only when a generated candidate row is
  inserted successfully, or when an expression such as `LAST_INSERT_ID(expr)`
  explicitly changes it
- statement warning records include 1287 `VALUES()` deprecation warnings and
  any demoted `IGNORE` warnings
- statement execution state tracks records, duplicates, and warnings for the
  eventual protocol OK packet

The duplicate count should increment for each source row that takes the update
branch, including no-op updates and `IGNORE`-demoted update failures. It should
not increment for ordinary inserted rows.

### Storage and transaction handling

Use one statement transaction or savepoint around physical row changes. The
transaction must support mixed row outcomes:

- ordinary inserted rows survive successful statement completion
- changed duplicate rows survive successful statement completion
- no-op duplicate rows leave storage unchanged but count as duplicates
- `IGNORE`-demoted update failures leave that row unchanged and allow later
  rows to continue
- fatal errors roll back all physical inserts and updates performed by the
  statement

Auto-increment metadata needs separate handling from row rollback. Generated
candidate values consumed before conflicts or demoted failures must remain
consumed according to the existing InnoDB-compatible MyLite policy.

SQLite's native upsert machinery is not sufficient by itself because MySQL
requires pre-update candidate validation, catalog-order conflict selection,
MySQL diagnostics, source-order expression semantics, and MySQL affected-row
counts. MyLite should perform explicit analysis and then issue targeted
physical insert/update operations.

### SQL modes

The first implementation should target the default MySQL 8.4.9 strict mode
used by compatibility tests. ODKU must preserve the existing insert and update
SQL-mode boundaries:

- candidate insert validation respects strict-mode errors unless `IGNORE`
  demotes a supported condition
- duplicate-key update assignment conversion uses the same conversion policy as
  single-table `UPDATE`
- `NO_AUTO_VALUE_ON_ZERO`, `auto_increment_increment`,
  `auto_increment_offset`, and replication-specific auto-increment behavior
  remain deferred

### Explicit deferred behavior

MyLite intentionally documents these boundaries for the first implementation:

- `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE` remains unsupported until the
  insert-from-query source is specified.
- Partition clauses remain unsupported until partition metadata and routing
  exist.
- Priority and deprecated delayed modifiers remain unsupported.
- Generated columns remain unsupported at runtime until generated-column DDL
  and execution exist; ODKU must not silently ignore generated-column effects.
- Triggers, foreign keys, views, privileges, and binary logging remain
  unsupported and should use existing deterministic placeholder diagnostics
  where parser acceptance would otherwise imply execution.
- Full conversion/range/truncation/temporal warning behavior follows the
  broader conversion roadmap and must not be claimed as fully supported if
  those slices are absent.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `INSERT INTO t VALUES (...) ON DUPLICATE KEY UPDATE ...` with no conflict | Inserts row; affected rows `1`; duplicate count `0`. |
| `INSERT INTO t VALUE (...) ON DUPLICATE KEY UPDATE ...` | `VALUE` synonym accepted. |
| `INSERT INTO t VALUES ROW(...), ROW(...) ON DUPLICATE KEY UPDATE ...` | Processes rows in source order. |
| `INSERT INTO t SET a=1 ON DUPLICATE KEY UPDATE ...` | SET form accepted. |
| duplicate branch changes a value | Existing row updated; affected rows contribution `2`; duplicate count increments. |
| duplicate branch assigns current values | Existing row unchanged; affected rows contribution `0`; duplicate count increments. |
| mixed insert/update/no-op rows | Affected rows sum uses `1`, `2`, and `0` per row. |
| repeated ODKU assignment target | Accepted; assignments run left to right. |
| `b=a+1, a=VALUES(a), c=b+1` | Later assignments see earlier writes; candidate value read through `VALUES`. |
| `VALUES(col)` | Candidate-row value used; warning 1287 per use. |
| row alias reference `new.a` | Candidate-row value used without 1287 warning. |
| column alias reference `alias_col` | Candidate-row value used when not ambiguous. |
| row alias same as target table | Error 1066 before mutation. |
| column alias count mismatch | Error 1353 before mutation. |
| duplicate column alias | Error 1060 before mutation. |
| unqualified alias/target ambiguity | Error 1052 before mutation. |
| unknown ODKU assignment target | Error 1054 before mutation. |
| candidate row omits required no-default column | Error 1364 before update branch. |
| candidate insert explicitly supplies generated column | Error 3105 before update branch. |
| candidate default read through `VALUES(defaulted_col)` | ODKU uses the candidate default value. |
| generated auto-increment candidate updates existing row | Generated id consumed; session last insert id unchanged unless previously set. |
| generated auto-increment candidate inserts new row | Session last insert id becomes the first generated inserted id. |
| explicit high duplicate candidate | Existing row updates; sequence does not advance merely because candidate id was high. |
| `LAST_INSERT_ID(id)` inside update assignment | Session last insert id changes through function semantics. |
| update branch duplicates another unique key without `IGNORE` | Error 1062; no rows from the statement survive. |
| same update-branch duplicate with `IGNORE` | Warning 1062; later source rows continue. |
| candidate conflicts with multiple unique indexes | Row selected by MySQL-compatible unique-index order. |
| nullable unique columns with `NULL` candidate key parts | `NULL` does not create a duplicate conflict. |
| update changes base column of generated stored column | Generated value recalculated once generated columns exist. |
| update no-ops through generated unique conflict | Affected rows contribution `0`. |
| `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE` | Deferred unsupported form until insert-from-query is specified. |
| `INSERT ... PARTITION (...) ... ON DUPLICATE KEY UPDATE` | Deferred unsupported form until partition routing is specified. |
| `INSERT LOW_PRIORITY ... ON DUPLICATE KEY UPDATE` | Deferred unsupported form until insert priorities are specified. |

## Test plan

Parser tests:

- ODKU on `VALUES`, `VALUE`, `VALUES ROW(...)`, explicit column lists, empty
  column lists, all-default rows, omitted `INTO`, and `SET`
- `IGNORE` combined with ODKU on scoped insert forms
- row aliases and column aliases after `VALUES` and `SET`
- `VALUES(col)` as an expression in ODKU assignments
- multiple ODKU assignments and repeated assignment targets
- malformed ODKU clauses: missing assignment list, missing `KEY`, missing
  `UPDATE`, trailing comma, missing `=`, and missing expression
- continued rejection for priority modifiers, `DELAYED`, partitions, and
  insert-from-query ODKU until those specs are implemented

Implemented runtime tests:

- successful insert path with ODKU present but no conflict
- duplicate update path with changed and unchanged rows
- mixed multi-row source with insert, update, and no-op update
- conflict detection against primary key, single unique key, multiple unique
  keys in MyLite catalog order, and update-branch duplicate rollback
- composite primary-key conflict detection
- nullable unique-key parts containing `NULL`
- assignment order, repeated target assignments, `DEFAULT`, target-column
  references, `VALUES(col)`, row aliases, and column aliases
- candidate-row defaults exposed through `VALUES(col)` and aliases
- alias diagnostics 1066, 1353, 1060, and 1052
- unknown ODKU assignment target diagnostics
- candidate insert validation before update through the existing insert
  validation tests plus ODKU rollback probes
- update-branch duplicate-key error with rollback
- `INSERT IGNORE` update-branch duplicate-key demotion and continuation
- `INSERT IGNORE` update-assignment numeric, string-length, and temporal
  coercion demotion with row-numbered warnings
- warning records for 1287 and demoted 1062, including warning ordering
- affected rows, warning count, session last insert id, and auto-increment
  catalog state
- fatal error rollback for all rows inserted or updated by the statement

Deferred runtime tests:

- composite unique-key ODKU conflict surfaces beyond the current primary,
  single-key, and multiple-index coverage
- generated-column validation and generated-column `DEFAULT` behavior
- triggers, foreign keys, views, partitions, priority/delayed modifiers, and
  insert-from-query ODKU sources
- conversion/range/truncation demotion under `IGNORE` beyond the covered
  update-assignment coercions
- explicit `LAST_INSERT_ID(expr)` behavior when the scalar function surface is
  available in ODKU expressions

MySQL-runtime comparison tests must verify result rows, warning codes/messages,
warning ordering, affected rows, duplicate counts, last insert id,
auto-increment catalog state, and absence of mutation for nonignorable errors.

## Implementation risks

- Conflict selection with multiple unique indexes is index-order-sensitive.
  SQLite upsert conflict resolution cannot be treated as MySQL-compatible
  without explicit catalog-order checks.
- Candidate insert validation must happen before the update branch. Skipping
  that work would accept rows MySQL rejects.
- `VALUES(col)` is easy to implement as a generic function call, but its ODKU
  candidate-row semantics and 1287 warnings are context-specific.
- ODKU assignment order allows repeated targets and requires a mutable row
  image. Reusing insert assignment duplicate checks would be wrong.
- Affected rows are not changed-row counts alone. Inserts, changed updates, and
  no-op updates each contribute different values.
- Auto-increment values can be consumed by duplicate updates without changing
  `LAST_INSERT_ID()`. The sequence allocator and session metadata must remain
  separate.
- `INSERT IGNORE` with ODKU requires row-continuing behavior for update-branch
  errors while preserving fatal rollback for nonignorable failures.
- Generated columns, triggers, and foreign keys affect both conflict detection
  and update side effects. Until those features exist, MyLite must report
  explicit gaps instead of marking the full surface supported.
