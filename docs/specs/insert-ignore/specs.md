# INSERT IGNORE

## Scope

This feature specifies MySQL-compatible `INSERT IGNORE` behavior for the insert
forms MyLite already supports:

- `INSERT [IGNORE] [INTO] table_name [(column_list)] VALUES ...`
- `INSERT [IGNORE] [INTO] table_name [(column_list)] VALUE ...`
- `INSERT [IGNORE] [INTO] table_name [(column_list)] VALUES ROW(...) ...`
- `INSERT [IGNORE] [INTO] table_name SET assignment_list`

Current implementation status:

- The first implementation slice supports `IGNORE` on the existing
  `INSERT ... VALUES` and `INSERT ... SET` parser/AST/runtime surfaces.
- Implemented demotions are primary/unique duplicate-key row skipping and
  required-column implicit defaults for explicit `NULL`, explicit `DEFAULT`,
  and omitted required no-default non-auto columns.
- The first slice records warnings through `mylite_warning_count()`,
  `mylite_warning_code()`, and `mylite_warning_message()` for MySQL codes
  1062, 1048, and 1364.
- Full data conversion, range clipping, string truncation, and invalid temporal
  value demotion remain deferred and must not be treated as supported by this
  slice.

In scope:

- parser and AST representation for the `IGNORE` modifier on the existing
  `INSERT ... VALUES` and `INSERT ... SET` statement nodes
- row-continuing execution for ignorable per-row errors
- duplicate primary-key and unique-key demotion to warnings
- `NOT NULL` explicit `NULL`, explicit `DEFAULT`, and omitted required-column
  demotion to implicit type defaults where MyLite has enough column type
  metadata to do so
- data conversion, range, truncation, and temporal warning behavior for the
  type families already represented by MyLite column metadata
- affected rows, warning counts, duplicate counts, session last insert id, and
  `AUTO_INCREMENT` side effects
- deterministic interaction with the current MyLite warning/diagnostic layer,
  even where protocol information strings are still deferred
- failure behavior for nonignorable validation errors

Out of scope:

- `INSERT ... SELECT`, `INSERT ... TABLE`, and standalone `VALUES`
- `INSERT ... ON DUPLICATE KEY UPDATE`
- `REPLACE`
- priority modifiers, `DELAYED`, partitions, and row/column aliases
- generated columns, triggers, foreign keys, privilege checks, and views
- `NO_AUTO_VALUE_ON_ZERO`, `auto_increment_increment`,
  `auto_increment_offset`, replication safety flags, and storage-engine
  variants other than MyLite's InnoDB-compatible choice
- exact protocol OK-packet text until MyLite exposes the MySQL wire protocol
  information string surface

## Sources

- MySQL 8.4 Reference Manual, `INSERT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, `INSERT ... ON DUPLICATE KEY UPDATE`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html
- Existing MyLite specs:
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/column-attributes/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849` with `mysql:8.4.9`, using:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --table --force --show-warnings -vvv
```

The verified server reported version `8.4.9`, default session SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
and `@@innodb_autoinc_lock_mode = 2`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 behavior summary

### Syntax

`IGNORE` appears after an optional insert priority modifier and before optional
`INTO`. For this task, MyLite should accept only the existing no-priority
surfaces:

```sql
INSERT IGNORE INTO t VALUES (1, 2);
INSERT IGNORE t VALUES ROW(1, 2), ROW(3, 4);
INSERT IGNORE INTO t SET a = 1, b = 2;
```

Priority modifiers, `DELAYED`, partitions, row aliases, column aliases, and
`ON DUPLICATE KEY UPDATE` are syntactically valid MySQL extensions around
`IGNORE`, but remain separate MyLite features because they affect scheduling,
warnings, partition routing, name resolution, and conflict handling.

### Ignorable and nonignorable conditions

`IGNORE` changes only selected execution-time conditions. Syntax errors,
missing target tables, missing schemas, unknown target columns, duplicate target
columns, and wrong row arity remain errors and must not mutate storage.

The scoped ignorable conditions are:

- duplicate primary-key and unique-key conflicts
- explicit `NULL` for `NOT NULL` columns
- explicit `DEFAULT` for required columns with no explicit default
- omitted required columns with no explicit default
- type conversion errors that MySQL can coerce to an implicit or clipped value
  in a later conversion slice
- numeric range overflow or underflow in a later conversion slice
- string truncation in a later conversion slice
- temporal invalid or zero values affected by strict SQL modes in a later
  conversion slice

Unsupported expressions, unsupported generated default expressions, allocation
failures, SQLite I/O failures, file corruption, and internal MyLite errors are
not ignorable.

### Duplicate-key rows

For `INSERT IGNORE`, a row that conflicts with an existing or earlier accepted
unique value is not inserted. Execution continues with the next row. Each
ignored duplicate row increments the statement duplicate count and, in the
verified probes, records warning 1062 with the duplicate-key message.

Observed `INSERT ... VALUES` example:

```sql
CREATE TABLE dup_ai(
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  v INT UNIQUE,
  nn INT NOT NULL,
  s VARCHAR(3) NOT NULL DEFAULT 'ok'
) ENGINE=InnoDB AUTO_INCREMENT=10;
INSERT INTO dup_ai(v, nn, s) VALUES (1, 10, 'one');
INSERT IGNORE INTO dup_ai(v, nn, s)
VALUES (1, 11, 'dup'), (2, 20, 'two'), (1, 12, 'dup'), (3, 30, 'tri');
```

MySQL inserted the two nonduplicate rows, reported affected rows `2`, duplicate
count `2`, warning count `2`, preserved the seed row, stored generated ids
`11` and `12`, set `LAST_INSERT_ID()` to `11`, and advanced the next
`AUTO_INCREMENT` value to `15`.

Observed `INSERT ... SET` duplicate example:

```sql
INSERT IGNORE INTO dup_ai SET v=2, nn=99, s='set';
```

MySQL inserted no row, reported affected rows `0`, warning count `1`, left
`LAST_INSERT_ID()` unchanged at `11`, and advanced the next auto-increment
value from `15` to `16`.

### Required columns and implicit defaults

When `IGNORE` demotes required-column failures, MySQL writes the column's
implicit type default:

- numeric types use `0`
- string and binary types use the empty value
- temporal types use the zero value for the target temporal type

Observed strict-mode probes:

```sql
CREATE TABLE null_req(id INT NOT NULL, s VARCHAR(3) NOT NULL) ENGINE=InnoDB;
INSERT IGNORE INTO null_req(id, s)
VALUES (1, 'abc'), (NULL, NULL), (2, 'toolong');
```

MySQL inserted all three rows, stored `(0, '')` for the explicit `NULL`
required columns, truncated `'toolong'` to `'too'`, reported affected rows `3`,
and recorded warnings 1048, 1048, and 1265.

Single-row `INSERT IGNORE` behaves the same for explicit `NULL` in the verified
server:

```sql
INSERT IGNORE INTO null_req(id, s) VALUES (NULL, NULL);
```

MySQL inserted one row containing implicit defaults and recorded two 1048
warnings.

Repeated explicit `NULL` values for the same required column in a multi-row
statement record one 1048 warning for that column, while all rows still receive
the implicit stored default.

For required columns omitted from the insert list:

```sql
CREATE TABLE missing_req(id INT NOT NULL, opt INT) ENGINE=InnoDB;
INSERT IGNORE INTO missing_req(opt) VALUES (10), (20);
```

MySQL inserted both rows with `id = 0` and recorded one 1364 warning for the
statement. Explicit `DEFAULT` for multiple required no-default columns records
one 1364 warning per column that needed an implicit default:

```sql
CREATE TABLE req(id INT NOT NULL, s VARCHAR(4) NOT NULL) ENGINE=InnoDB;
INSERT IGNORE INTO req(id, s) VALUES (DEFAULT, DEFAULT);
INSERT IGNORE INTO req SET id=DEFAULT, s=DEFAULT;
```

Each statement inserted one row `(0, '')` and recorded two 1364 warnings.

### Conversion, range, truncation, and temporal values

`IGNORE` takes precedence over strict SQL modes for data-change validation.
Values that would otherwise fail under strict mode are coerced to MySQL's
chosen stored value and recorded as warnings.

Observed probe:

```sql
CREATE TABLE conv_range(
  i TINYINT NOT NULL,
  u TINYINT UNSIGNED NOT NULL,
  d DATE NOT NULL
) ENGINE=InnoDB;
INSERT IGNORE INTO conv_range
VALUES ('abc', 999, '2024-02-31'), (-200, -1, '0000-00-00');
```

MySQL inserted two rows:

| i | u | d |
| --- | --- | --- |
| `0` | `255` | `0000-00-00` |
| `-128` | `0` | `0000-00-00` |

It recorded six warnings: 1366 for the nonnumeric integer input and 1264 for
the out-of-range unsigned, date, signed tinyint, unsigned tinyint, and zero-date
values in row order.

MyLite must eventually route all existing scalar coercion through a common
conversion API that can return both the coerced value and a warning condition.
For the implementation slice covered by this spec, conversion support should be
limited to column types already described in MyLite metadata. Unsupported
conversion domains must remain explicit MyLite unsupported diagnostics rather
than silently accepting wrong data.

### Affected rows and diagnostics

Successful `INSERT IGNORE` reports affected rows as the number of rows actually
inserted. Ignored duplicate rows do not contribute to affected rows. Rows
inserted with coerced values do contribute.

For multi-row `INSERT ... VALUES` and `VALUES ROW(...)`, MySQL's OK information
string includes processed-record, duplicate, and warning counts. MyLite should
track these counts in the statement execution state even if only affected rows
and warnings are initially exposed through the public API. The eventual protocol
layer should use the same state to render MySQL-compatible OK-packet
information.

Warnings are statement-owned and ordered by row processing and then by column
or condition within the row. Failed nonignorable statements leave an error
diagnostic and do not convert the statement into a warning-only success.

### Last insert id and AUTO_INCREMENT

`LAST_INSERT_ID()` is set to the first automatically generated auto-increment
value from an accepted row. Ignored rows do not set it. Explicit nonzero
auto-increment values do not set it.

MyLite should follow the existing insert specs' InnoDB-compatible sequence
choice: generated values consumed by failed or ignored rows are not reused.
This matches the verified MySQL 8.4.9 InnoDB server with
`@@innodb_autoinc_lock_mode = 2`.

Observed sequence effects:

- A four-row `INSERT IGNORE` with two duplicate rows and two accepted rows
  starting at `AUTO_INCREMENT=11` advanced the next value to `15`.
- A single-row `INSERT IGNORE ... SET` duplicate advanced the next value by
  one and left `LAST_INSERT_ID()` unchanged.
- A mixed insert beginning at `AUTO_INCREMENT=100`:

  ```sql
  INSERT IGNORE INTO atomic_probe(v, nn)
  VALUES (1, 1), (1, 2), (2, NULL), (3, 3);
  ```

  inserted three rows with ids `100`, `101`, and `102`, skipped the duplicate
  row, coerced `NULL` to `0` for `nn`, set `LAST_INSERT_ID()` to `100`,
  recorded warnings 1062 and 1048, and advanced the next value to `104`.

### Atomicity and continuation

Ordinary `INSERT ... VALUES` is statement-atomic in the existing MyLite spec:
a late duplicate or validation error rolls back the whole statement. `INSERT
IGNORE` is intentionally different for ignorable per-row failures. Accepted
rows before and after an ignored row remain committed when the statement
finishes successfully.

Implementation must still be atomic for nonignorable failures. If an
unsupported expression, missing table, unknown column, wrong arity, allocation
failure, or storage failure occurs, the statement must roll back rows inserted
by that MyLite statement. Auto-increment sequence side effects that MySQL would
have consumed before the failure should remain consumed only where existing
MyLite insert semantics already specify that behavior.

### SQL modes

The default MySQL 8.4.9 SQL mode includes strict data-change modes and zero-date
checks. `IGNORE` takes precedence over strict-mode escalation for the scoped
ignorable conditions. Clearing `sql_mode` does not make `NULL` into a `NOT
NULL` column acceptable without `IGNORE` in the verified server; MySQL produced
an error 1048. With `IGNORE`, both strict and relaxed mode probes inserted an
implicit default and recorded warnings.

MyLite's first implementation should target the default strict MySQL 8.4.9 mode
already used by compatibility tests. It should make the demotion mechanism
aware of SQL-mode state so later SQL-mode work can adjust warning/error
classification without rewriting insert execution.

### Unsupported interaction points

`IGNORE` must not accidentally enable unsupported insert features outside the
scoped implemented surfaces:

- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE` is implemented for the current
  `VALUES`, `VALUE`, `VALUES ROW(...)`, and `SET` forms by the ODKU feature.
  It demotes duplicate-key conflicts raised by the update branch to warning
  1062 and continues with later rows.
- `INSERT LOW_PRIORITY IGNORE`, `INSERT HIGH_PRIORITY IGNORE`, and
  `INSERT DELAYED IGNORE` remain deferred until insert modifiers are specified.
- `INSERT IGNORE ... PARTITION (...)` remains deferred until partition routing
  and partition-mismatch diagnostics are specified.
- `INSERT IGNORE ... AS row_alias[(column_alias,...)]` is implemented where it
  is part of the scoped ODKU surface.
- Generated columns, triggers, foreign keys, and view checks are future
  demotion surfaces; this task must leave deterministic unsupported diagnostics
  for those features.

## MyLite design

### Parser and AST

Extend the existing `INSERT ... VALUES` and `INSERT ... SET` AST nodes with an
`ignore` flag. Do not create separate statement node kinds unless the current
AST shape already uses modifier-specific nodes elsewhere. The runtime should be
able to share all non-`IGNORE` validation and insert paths.

The parser must accept `IGNORE` between `INSERT` and optional `INTO` for the
scoped no-priority forms. Unsupported priority modifiers must not be partially
accepted in this task.

### Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature and are
independently authored for MyLite's parser:

```lemon
insert_values_statement ::= INSERT opt_insert_ignore opt_into table_name
                            opt_insert_column_list insert_values_keyword
                            insert_row_list.

insert_set_statement ::= INSERT opt_insert_ignore opt_into table_name SET
                         insert_set_assignment_list.

opt_insert_ignore ::= .
opt_insert_ignore ::= IGNORE.
```

The existing `insert_values_keyword`, `insert_row_list`,
`opt_insert_column_list`, and `insert_set_assignment_list` productions from the
`INSERT ... VALUES` and `INSERT ... SET` specs remain unchanged.

The following MySQL-valid forms remain deliberately outside this task:

```lemon
/* Deferred: priority and deprecated delayed modifiers. */
insert_values_statement ::= INSERT insert_priority opt_insert_ignore opt_into
                            table_name opt_insert_column_list
                            insert_values_keyword insert_row_list.

/* Deferred: partition routing. */
insert_values_statement ::= INSERT opt_insert_ignore opt_into table_name
                            insert_partition_clause opt_insert_column_list
                            insert_values_keyword insert_row_list.

/* Deferred: aliases and duplicate-key update. */
insert_values_statement ::= INSERT opt_insert_ignore opt_into table_name
                            opt_insert_column_list insert_values_keyword
                            insert_row_list insert_row_alias
                            insert_duplicate_key_update_clause.
```

### Analyzer and execution model

Execution should reuse the existing `INSERT ... VALUES` and `INSERT ... SET`
pipeline with an `ignore` mode carried in statement execution state:

1. Resolve target schema/table and reject nonignorable statement-level errors.
2. Resolve column lists or assignment targets and reject nonignorable
   structural errors before mutation.
3. Build candidate rows in source order, preserving existing default,
   assignment-order, and auto-increment rules.
4. Convert each candidate value to the target column domain through a
   warning-capable conversion helper.
5. Validate `NOT NULL`, default availability, and duplicate keys.
6. Insert accepted rows and append warning records for ignorable conditions.
7. Commit accepted rows if no nonignorable error occurs; otherwise roll back the
   statement's physical rows.

The executor needs a per-row status:

| Status | Meaning |
| --- | --- |
| accepted | row was physically inserted and counts as affected |
| ignored | row was skipped because of an ignorable row-level condition |
| coerced | row was inserted after one or more ignorable value conditions |
| fatal | statement must fail and roll back physical rows |

`coerced` is compatible with `accepted`; it is separated here only to make the
warning path explicit.

### Warning and diagnostic model

MyLite needs statement-owned warning records before `INSERT IGNORE` can be
considered supported. Each warning should store at least:

- level, initially always `Warning` for demoted `IGNORE` conditions
- MySQL numeric code
- SQLSTATE when available from the diagnostic catalog
- message text
- source span or row/column context when available for internal tests

The initial warning codes required by this feature are:

| Condition | Code | Expected behavior |
| --- | --- | --- |
| duplicate primary or unique key | 1062 | skip row and continue |
| explicit `NULL` for `NOT NULL` | 1048 | store implicit default and continue |
| required no-default column omitted/defaulted | 1364 | store implicit default and continue |
| string too long for target string/blob family | 1265 | store truncated value and continue |
| numeric or temporal out of range | 1264 | store clipped or zero value and continue |
| invalid integer text | 1366 | store coerced numeric zero and continue |

The warning system should preserve multiple warnings for one row when MySQL
does. Duplicate-key warnings are one warning per ignored duplicate row.
Required-column omission warnings are statement-level in the verified
multi-row omission probe: two rows missing the same required column produced one
1364 warning, not one per row. Explicit `DEFAULT` or explicit `NULL` warnings
are per affected column in the verified probes.

### Duplicate-key demotion

Duplicate detection should run after candidate value resolution and
auto-increment allocation for the row. If a duplicate is found in `IGNORE` mode:

- append warning 1062
- increment duplicate count
- do not insert the row
- do not update session last insert id from that row
- preserve any generated auto-increment value consumed for that row
- continue with following rows

Duplicate detection must include conflicts with rows accepted earlier in the
same statement. It must use MyLite's current primary/unique key coverage and
later expand as index enforcement becomes more complete.

### Required-column and default demotion

In non-`IGNORE` mode, the existing insert diagnostics remain unchanged. In
`IGNORE` mode:

- explicit `NULL` for a required non-auto column appends warning 1048 and stores
  the implicit type default
- explicit `DEFAULT` for a required no-default column appends warning 1364 and
  stores the implicit type default
- an omitted required no-default column appends warning 1364 and stores the
  implicit type default

The implementation must centralize implicit default construction by MyLite
column type so `INSERT ... VALUES`, `INSERT ... SET`, future `LOAD DATA`, and
future `UPDATE IGNORE` can share behavior.

For auto-increment columns, omitted, `NULL`, `0`, and `DEFAULT` retain the
existing generated-value behavior rather than using an ordinary implicit
default.

### Conversion and range demotion

The current `INSERT ... VALUES` and `INSERT ... SET` specs defer full MySQL type
conversion. `INSERT IGNORE` should not be marked supported until the conversion
paths needed by this feature can emit MySQL-compatible warnings and store
compatible values for the currently supported column types.

Deferred conversion slices:

- signed and unsigned integer clipping
- string-to-integer conversion for invalid and partially numeric strings
- string and binary truncation to declared length
- date/time zero value insertion for invalid strict-mode temporal values where
  the temporal type is already supported by MyLite

The first implementation slice only uses type metadata to construct implicit
defaults for required-column `NULL`, `DEFAULT`, and omission demotions. It does
not demote invalid input values, out-of-range values, or truncation.

If the implementation lands before broad conversion support, the compatibility
row must remain partial with explicit gaps, not fully supported.

### Affected rows, counts, and public API

`mylite_affected_rows(stmt)` should report accepted rows only. Ignored duplicate
rows and skipped partition rows do not count. Coerced rows count because they
are inserted.

The statement execution object should also track:

- processed record count
- duplicate count
- warning count

Even if MyLite does not expose all three through public APIs immediately, tests
should be able to inspect warning records through the diagnostics API once that
API exists. The protocol layer should later format these counters for OK
packets.

### Last insert id and sequence side effects

Reuse the current MyLite auto-increment allocator. In `IGNORE` mode:

- set session last insert id to the first generated value from an accepted row
- leave session last insert id unchanged if all generated rows are ignored
- preserve explicit nonzero auto-increment behavior
- advance the table's next auto-increment value for every generated candidate
  row in the statement, including ignored duplicate rows

This is a MyLite compatibility decision aligned with verified MySQL 8.4.9
InnoDB behavior. MyLite does not need to emulate nontransactional engines whose
ignored rows may not advance the counter.

### Storage and transaction handling

Use one statement transaction or savepoint for the physical writes. The
transaction must allow accepted rows to survive ignorable row failures but roll
back all rows inserted by the statement on a fatal error. A practical shape is:

- begin the statement savepoint
- for each row, allocate generated values, evaluate/coerce values, and either
  insert or mark ignored
- on fatal error, roll back to the statement savepoint
- on success, release the savepoint and keep accepted rows

Auto-increment metadata updates need care because generated values consumed by
ignored rows must survive a successful statement, and generated values consumed
before a fatal error should follow the existing non-`IGNORE` insert policy.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `INSERT IGNORE INTO t VALUES (...)` | Accepted for the existing `VALUES` surface. |
| `INSERT IGNORE t VALUE (...)` | Accepted without `INTO` and with `VALUE` synonym. |
| `INSERT IGNORE INTO t VALUES ROW(...), ROW(...)` | Accepted for row constructors. |
| `INSERT IGNORE INTO t SET a=1` | Accepted for the existing `SET` surface. |
| duplicate existing unique value in multi-row `VALUES` | Duplicate rows skipped; later rows continue; warning 1062 per skipped row. |
| duplicate existing unique value in `SET` | Row skipped; affected rows `0`; warning 1062; generated auto id consumed. |
| duplicate against earlier accepted row in same statement | Later duplicate skipped; earlier row remains inserted. |
| nullable unique column with repeated `NULL` | Repeated `NULL` values are accepted and are not duplicates. |
| explicit `NULL` into required numeric/string columns | Row inserted with implicit defaults; warning 1048 per affected column. |
| single-row explicit `NULL` under default strict mode | Row inserted with implicit defaults; warning 1048. |
| omitted required no-default column | Rows inserted with implicit default; one 1364 warning for the omitted column in verified multi-row probe. |
| explicit `DEFAULT` for required no-default columns | Row inserted with implicit defaults; warning 1364 per affected column. |
| string too long for `VARCHAR(3)` | Truncated value stored; warning 1265. |
| invalid integer text | Numeric zero stored; warning 1366. |
| signed/unsigned integer range overflow | Clipped endpoint stored; warning 1264. |
| invalid or strict-disallowed date | Zero date stored where MySQL stores it; warning 1264 in verified date probe. |
| mixed duplicate plus coerced required value | Accepted/coerced rows remain; duplicate row skipped; affected rows count accepted rows only. |
| all rows duplicates after a previous successful generated insert | Affected rows `0`; last insert id unchanged; generated ids consumed. |
| first generated candidate ignored, later generated row accepted | Last insert id becomes first generated value from the accepted row, not the ignored row. |
| nonignorable unknown target column | Error 1054; no mutation. |
| duplicate target column in insert column list | Error 1110; no mutation. |
| wrong row arity | Error 1136; no mutation. |
| missing target table | Error 1146; no mutation. |
| unsupported expression during value evaluation | Fatal MyLite diagnostic; statement rows rolled back. |
| `INSERT LOW_PRIORITY IGNORE ...` | Deferred unsupported form until insert priorities are specified. |
| `INSERT IGNORE ... PARTITION (...)` | Deferred unsupported form until partitions are specified. |
| `INSERT IGNORE ... AS new` | Accepted for the scoped ODKU insert surfaces; otherwise no runtime effect. |
| `INSERT IGNORE ... ON DUPLICATE KEY UPDATE ...` | Implemented for current `VALUES`/`SET` forms; update-branch duplicate conflicts are warning-demoted and later rows continue. |

## Test plan

Parser tests:

- `IGNORE` on `VALUES`, `VALUE`, `VALUES ROW(...)`, all-default rows, explicit
  column lists, empty column lists, and omitted `INTO`
- `IGNORE` on `SET` assignment lists
- malformed placements such as `INSERT INTO IGNORE t ...`
- continued parse rejection for priority modifiers, `DELAYED`, and partitions
- scoped alias and ODKU coverage when combined with `IGNORE`

Runtime tests:

- duplicate-key skipping for `VALUES` and `SET`
- duplicate-key conflicts against existing rows and rows accepted earlier in
  the same statement
- nullable unique `NULL` values
- required-column explicit `NULL`, explicit `DEFAULT`, and omission
- implicit defaults for signed/unsigned integers, strings/binary values, and
  temporal values supported by MyLite
- conversion warnings for invalid integer text, partial numeric text, range
  clipping, string truncation, invalid dates, zero dates, and division by zero
  once expression evaluation supports it
- affected rows, warning records, duplicate count, processed records, and
  session last insert id
- auto-increment sequence advancement for accepted rows, skipped duplicates,
  explicit high values, all-duplicate statements, and mixed accepted/skipped
  statements
- fatal error rollback for nonignorable validation and storage failures
- SQL-mode tests for default strict mode plus a focused relaxed-mode probe once
  MyLite exposes session `sql_mode`

MySQL-runtime comparison tests must verify result rows, warning codes/messages,
warning ordering, affected rows, last insert id, auto-increment catalog state,
and absence of mutation for nonignorable errors.

## Implementation risks

- `INSERT IGNORE` cannot be correct without a first-class warning list. A
  single last-error slot is insufficient.
- Current insert code is statement-atomic for many failures. `IGNORE` requires
  row-continuing success while still preserving fatal rollback.
- Auto-increment metadata updates must be decoupled from physical row rollback
  so ignored generated rows consume ids.
- Required-column omission warning counts are not always per row. Tests should
  lock down the exact behavior before broadening the implementation.
- SQLite uniqueness errors alone are too late and too coarse for MySQL warning
  ordering and duplicate counts. MyLite should keep explicit duplicate checks
  before physical insertion.
- Type conversion support can easily undercount warnings or store SQLite-native
  values that differ from MySQL's clipped/zero/truncated values.
