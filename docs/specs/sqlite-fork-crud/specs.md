# SQLite Fork CRUD Foundation

## Status

This is the first design package for the SQLite-fork experiment. It does not
replace the existing MyLite parser, catalog, or runtime. It records the
primitive layer MyLite needs when MySQL compatibility is moved closer to
SQLite's parser, schema builder, VDBE, constraints, collations, and function
registry.

The implementation is still experimental, but it now covers two executable
slices:

- register MyLite's current MySQL collation names directly with SQLite
- register a few MySQL-compatible scalar functions directly with SQLite
- enable SQLite's native foreign-key enforcement for configured MyLite
  connections
- add a native truncate helper that preserves SQLite's fast row deletion path
  and resets `sqlite_sequence` for auto-increment tables
- add a WordPress-like CRUD test using native SQLite tables shaped as the
  intended physical form of MySQL-compatible tables
- configure these primitives for real MyLite connections
- emit safer native physical tables for supported MyLite DDL: rowid-backed
  single-column integer `AUTO_INCREMENT` primary keys, MySQL collation names,
  physical defaults, and secondary SQLite indexes where prefix semantics do not
  make a physical index too strict
- track native MySQL statement/session state for the direct fork subset,
  including `ROW_COUNT()`, `LAST_INSERT_ID()`, generated rowids, result-set
  completion, and `TRUNCATE` affected-row semantics
- execute the MySQL-runtime-verified WordPress-like CRUD fixture through
  MyLite's public SQL API

The current slice proves an end-to-end MyLite path for the fixture, but it is
not yet a claim that SQLite's own parser accepts the MySQL script unchanged.
MyLite still parses the MySQL SQL text and lowers it into SQLite operations.
The convergence target remains that the script in this directory should
eventually execute unchanged through the forked SQLite parser.

## Sources

- MySQL 8.4 Reference Manual, Data Types:
  https://dev.mysql.com/doc/refman/8.4/en/data-types.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `INSERT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `UPDATE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/update.html
- MySQL 8.4 Reference Manual, `DELETE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/delete.html
- MySQL 8.4 Reference Manual, `TRUNCATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/truncate-table.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- SQLite Datatypes:
  https://www.sqlite.org/datatype3.html
- SQLite `CREATE TABLE`:
  https://www.sqlite.org/lang_createtable.html
- SQLite STRICT Tables:
  https://www.sqlite.org/stricttables.html
- SQLite application-defined SQL functions:
  https://www.sqlite.org/c3ref/create_function.html
- SQLite collating sequences:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite foreign key support:
  https://www.sqlite.org/foreignkeys.html
- SQLite fork extension point map:
  `docs/specs/sqlite-fork-extension-point-map/specs.md`

This specification is independently authored from official documentation,
observed MySQL 8.4.9 runtime behavior, and SQLite's public documentation. It
does not copy MySQL, MariaDB, Percona, or other restrictively licensed
implementation sources.

## Problem Statement

The current MyLite implementation proves a large amount of MySQL behavior, but
too much of the execution surface lives beside SQLite instead of inside SQLite.
The result is correct in many places, but the architecture repeats features
SQLite already executes well: scans, joins, CTEs, window functions,
constraints, expression bytecode, transaction scopes, and statement atomicity.

The fork experiment should make SQLite carry the parts it is already excellent
at carrying, then patch or extend SQLite only where MySQL semantics need a
different primitive. Translation should become a thin compatibility boundary,
not a second SQL engine.

## Existing MyLite Knowledge To Reuse

The existing implementation and specs should remain the compatibility oracle.
The fork path should reuse the independent MySQL behavior research already
captured in these areas:

- column type descriptors for integer, boolean, string, binary, exact numeric,
  approximate numeric, and temporal declarations, including the initial
  SQLite-fork `DATE`, `DATETIME`, `TIME`, and `YEAR` assignment descriptors
- schema, table, column, and index metadata shape
- `CREATE TABLE`, `DROP TABLE`, `TRUNCATE TABLE`, standalone indexes, and
  table/index alterations
- `INSERT`, `INSERT IGNORE`, `REPLACE`, and `ON DUPLICATE KEY UPDATE`
- single-table and joined `UPDATE`
- single-table and multi-table `DELETE`
- scalar, aggregate, JSON, regexp, temporal, UUID, and advisory-lock functions
- `SELECT`, joins, grouping, ordering, limits, subqueries, unions, and
  diagnostics
- SQL modes, warnings, affected rows, last insert id, and transaction behavior

This knowledge should move downward into SQLite primitives instead of being
discarded.

## MySQL 8.4.9 Behavior Baseline

Runtime probes for this feature were executed on 2026-05-06 against the
official `mysql:8.4.9` Docker image in container `mylite-mysql-849`, using
MySQL's default strict SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

The first WordPress-like probe intentionally uses a valid `DATETIME DEFAULT
CURRENT_TIMESTAMP`. A direct `DEFAULT '0000-00-00 00:00:00'` probe fails under
the default MySQL 8.4.9 strict mode with error 1067, `Invalid default value for
'post_date'`.

For the verified script in `mysql-wordpress-like.sql`, MySQL produced the
following result rows:

```text
posts-before-truncate	3	1	3	4
published	1	hello-mylite	publish	2
published	2	draft-notes	publish	1
published	3	sqlite-fork-plan	publish	1
meta-before-truncate	2	1:_thumbnail_id=99|3:_wp_page_template=default
meta-after-truncate	0	0
meta-after-reinsert	1	2	_restored	yes
remaining-tables	1
```

The behavior that matters for the first CRUD foundation is:

- `CREATE TABLE` accepts MySQL type names, table options, primary keys, and
  secondary indexes.
- `AUTO_INCREMENT` allocates ids starting at `1` for both tables.
- `UPDATE` evaluates `comment_count = comment_count + 1` per matching row.
- `DELETE` removes only rows matching the predicate.
- `TRUNCATE TABLE` empties the table, reports no row count requirement for the
  deleted rows, and resets the next `AUTO_INCREMENT` value.
- `ROW_COUNT()` reports write affected rows, reports `-1` after a completed
  result-producing `SELECT`, and reports `0` after `TRUNCATE`.
- `LAST_INSERT_ID()` reports the first generated `AUTO_INCREMENT` value for
  successful inserts and can be explicitly set with `LAST_INSERT_ID(expr)`.
- `DROP TABLE` removes the table and its metadata.

Configured MyLite fork connections enable SQLite's native foreign-key
enforcement with SQLite's public database-configuration API. Foreign-key DDL
parsing and catalog integration remain separate MyLite work, but the execution
primitive should not require every caller to remember a per-connection PRAGMA.

## SQLite Capabilities To Lean On

SQLite should remain responsible for:

- B-tree storage, row lookup, indexes, uniqueness checks, sorting, grouping,
  joins, CTEs, window execution, and transaction rollback.
- The VDBE expression runtime wherever MySQL-compatible functions, casts,
  collations, and comparison behavior have been registered or patched.
- Native `CHECK`, `NOT NULL`, `UNIQUE`, `PRIMARY KEY`, and foreign-key
  constraint machinery after MySQL's exact diagnostics and timing have been
  wired in.
- `sqlite3_create_function_v2()` and `sqlite3_create_window_function()` for
  MySQL functions that can be implemented as compact C callbacks.
- `sqlite3_create_collation_v2()` for MySQL collation names when the comparison
  algorithm fits in MyLite's binary-size budget.
- Rowid tables for single-column auto-increment primary keys where MySQL's
  visible type can be represented separately from the physical rowid key.

SQLite's flexible declared-type model is useful for preserving authored MySQL
types in schema text, but it is not sufficient by itself. MySQL compatibility
needs stricter conversion, range, length, signedness, collation, default, and
diagnostic behavior than ordinary SQLite affinity provides.

## Required Fork Primitives

### Parser and grammar

The long-term fork should add MySQL syntax to SQLite's Lemon grammar instead of
preprocessing SQL text:

- MySQL type declarations and attributes: `UNSIGNED`, `ZEROFILL`, display
  widths, character-set clauses, collations, `AUTO_INCREMENT`, comments, and
  generated-column forms.
- MySQL DDL table options: `ENGINE`, `DEFAULT CHARSET`, `COLLATE`, `COMMENT`,
  `AUTO_INCREMENT`, and later ignored or diagnostic-only options.
- MySQL DML forms: `INSERT ... SET`, `INSERT ... ON DUPLICATE KEY UPDATE`,
  `REPLACE`, `UPDATE ... JOIN`, multi-table `DELETE`, `DELETE ... ORDER BY
  ... LIMIT`, and `TRUNCATE TABLE`.
- MySQL identifier, string-literal, versioned-comment, and SQL-mode grammar
  variants.

The parser must produce enough metadata for SQLite's normal schema builder and
VDBE generator to make native decisions. The target is not a large AST outside
SQLite; it is MySQL-aware SQLite parse nodes.

### Type descriptors and storage classes

MySQL visible types should be represented as a compact type descriptor attached
to SQLite `Column` metadata:

- canonical MySQL type family and display text
- signedness, numeric range, decimal precision/scale, string length, character
  set, collation, temporal fractional seconds precision, JSON marker, and
  binary/text distinction
- flags for `AUTO_INCREMENT`, `ZEROFILL`, generated columns, implicit default,
  nullable, invisible, and compatibility aliases

Physical storage should use SQLite's existing serial types where possible:

- rowid/`INTEGER PRIMARY KEY` for auto-increment integer primary keys when the
  MySQL range fits the signed rowid path
- `INTEGER` for signed integer families that fit signed 64-bit values
- an explicit unsigned or decimal representation for values that do not fit
  signed 64-bit integer semantics
- `TEXT` for MySQL character data after connection character-set conversion
- `BLOB` for binary strings
- compact canonical `TEXT` or integer encodings for temporal values until a
  stronger temporal storage primitive is specified
- canonical JSON text first, with a later binary JSON decision only after size,
  semantics, and license implications are evaluated

SQLite STRICT tables are not a complete solution because their allowed type
names and conversion rules are SQLite-specific. A MyLite fork can borrow the
same enforcement location but needs MySQL type names and MySQL conversion
rules.

### Conversion and diagnostics

Value conversion must happen in the SQLite insert/update path before values
are written. MySQL-compatible behavior needs:

- strict and non-strict SQL mode handling
- numeric clipping or errors with warnings where MySQL warns
- string length checks in characters for character strings and bytes for binary
  strings
- temporal parsing, invalid-date handling, fractional precision rounding or
  truncation, and zero-date mode checks
- exact error codes, SQLSTATEs, warning counts, affected rows, and rollback
  timing

This should be a patched VDBE/check-constraint path, not ad hoc validation in
each statement executor.

### Collations

The fork must register MySQL collation names natively so SQLite can use them in
indexes, equality, ordering, grouping, `DISTINCT`, and uniqueness checks. The
first executable slice registers MyLite's current collation names and implements
binary plus ASCII case-insensitive comparison with pad-space handling. MyLite
generated SQL now explicitly preserves column collation on prefix-key
expressions used by unique probes and existing-row duplicate validation. That
is enough for the verified ASCII WordPress-like and prefix-unique fixtures, but
it is not a full Unicode collation implementation.

Full `utf8mb4_0900_ai_ci`, `utf8mb4_unicode_ci`, and related collations remain
one of the most important hard problems. They should be solved either by a
small independently authored collation implementation for the needed subset or
by a carefully reviewed dependency if the size and license cost are justified.

### Functions

MySQL scalar, aggregate, and window functions should be native SQLite functions
where the VDBE can call them directly. The existing MyLite function behavior
specs should guide implementation. Function descriptors need:

- MySQL name and aliases
- arity and argument conversion rules
- result type and collation inference
- deterministic/direct-only/innocuous flags where appropriate
- warning and error reporting hooks into the MyLite diagnostics area
- statement and session state access for `NOW()`, `DATABASE()`,
  `LAST_INSERT_ID()`, `ROW_COUNT()`, user variables, and locks

The current executable slice registers compact callbacks for `CONCAT`,
`CONCAT_WS`, `IF`, `BIT_LENGTH`, `BIT_COUNT`, `DATABASE`, `SCHEMA`,
`LAST_INSERT_ID`, `ROW_COUNT`, `ISNULL`, `STRCMP`, `LENGTH`, `OCTET_LENGTH`,
`CHAR_LENGTH`, and `CHARACTER_LENGTH`. `IF()` also proves the first
successful-statement scalar warning path by publishing MySQL warning 1292 for
truncated numeric condition conversion. `DATABASE()` and `SCHEMA()` prove a
connection-local session-state path using SQLite client data.
`LAST_INSERT_ID()` and `ROW_COUNT()` prove the first VDBE completion state
path: callbacks expose the values, but SQLite-fork statement halt logic records
previous row counts and generated auto-increment identity only after statement
success, rollback, read completion, or direct `TRUNCATE` lowering is known.
`ISNULL()` proves the first narrow parser admission hook for a MySQL function
name that SQLite otherwise tokenizes as syntax before function lookup.
`STRCMP()` proves a native comparison callback where the first slice can reuse
MyLite's current ASCII case-insensitive PAD SPACE text comparison and bytewise
binary-string comparison. Broader function families stay in public SQLite
scalar callbacks until they need parser, broader statement-state, multi-warning
diagnostics, or storage hooks that SQLite's public function API cannot provide.

### Operators

MySQL operators that SQLite already tokenizes should use existing expression
nodes when their semantics match. MySQL-only operators need fork tokenizer and
parser admission because the public extension surface is too late.

The current executable slice recognizes scalar `<=>` in the SQLite fork and
lowers it to a compact runtime primitive. The covered behavior is:

- `NULL <=> NULL` returns `1`; one `NULL` returns `0`.
- Mixed numeric comparisons coerce text with MySQL-style leading-number parsing
  and warning 1292 on truncation.
- Default text comparison is ASCII case-insensitive and trims pad spaces.
- Binary-string comparison is bytewise.

Full collation-weight comparison, row `<=>`, optimizer/index mapping, and exact
MySQL warning text remain deferred.

### Auto-increment

SQLite rowid allocation is the correct starting point for MySQL
`AUTO_INCREMENT`, but MySQL-visible semantics need patches:

- MySQL accepts integer families other than exact `INTEGER` as
  `AUTO_INCREMENT`; the fork should map qualifying single-column primary keys
  to rowid storage while retaining the visible MySQL type descriptor.
- MySQL's next value, first generated id, failed multi-row insert gaps,
  explicit high value advancement, `NO_AUTO_VALUE_ON_ZERO`, and truncate reset
  must be modeled.
- Unsigned `BIGINT` values beyond signed rowid range need a documented storage
  path before support is claimed.

The first executable slice uses SQLite `AUTOINCREMENT` for the physical
WordPress-like tables and a helper that deletes from `sqlite_sequence` during
truncate. The fork now also records the first generated `AUTOINCREMENT` rowid
inside `OP_NewRowid` and publishes it to MySQL `LAST_INSERT_ID()` state from
`sqlite3VdbeHalt()` after successful statement completion.

### Statement state

`ROW_COUNT()` and `LAST_INSERT_ID()` cannot be implemented transparently with
public scalar callbacks alone. A callback can read connection state, but it
cannot know whether the previous VDBE program was a write, a completed result
set, a failed statement, a rolled-back statement, or direct `TRUNCATE`. The
fork stores compact MyLite state on `sqlite3`, tracks statement-local generated
rowids on `Vdbe`, and updates the connection state during VDBE halt. This also
keeps SQLite internal schema-initialization statements from changing
MySQL-visible session state.

### Metadata

SQLite schema metadata should be the primary storage for table/index structure,
but MySQL-visible metadata needs additional compact records:

- schema defaults
- original and canonical MySQL type text
- table options and comments
- index visibility, prefix lengths, order, comments, and key names
- `AUTO_INCREMENT` state where SQLite's sequence table is insufficient
- `INFORMATION_SCHEMA`, `SHOW`, and protocol metadata fields

This metadata should be updated by SQLite DDL paths, not a parallel DDL engine.

## Initial Implementation Plan

1. Add a fork primitive module that configures SQLite connections with MySQL
   collation and function names. Implemented.
2. Add native truncate support over SQLite tables, including `sqlite_sequence`
   reset. Implemented for the public helper and direct fork parser
   `TRUNCATE [TABLE] name`.
3. Add the MySQL-verified WordPress-like CRUD fixture and expected result rows.
   Implemented.
4. Add a native SQLite-fork test that creates the physical form of the same
   tables, runs equivalent CRUD, and checks the result rows. Implemented.
5. Configure real MyLite connections with the fork primitives and make
   supported `CREATE TABLE` output use native SQLite collations,
   rowid-backed `AUTO_INCREMENT`, defaults, and physical indexes where safe.
   Implemented for the first supported subset.
6. Add a public MyLite SQL fixture test for the WordPress-like CRUD script.
   Implemented.
7. Preserve MySQL column collation in generated prefix-unique expressions for
   `INSERT`, duplicate update, `UPDATE`, `CREATE UNIQUE INDEX`, and
   `ALTER TABLE ... ADD UNIQUE`. Implemented.
8. Next, move from MyLite-lowered SQL to direct MySQL SQL by forking the
   SQLite grammar for `TRUNCATE TABLE`, MySQL table options, secondary-key
   table elements, and `AUTO_INCREMENT`. Implemented for direct
   `TRUNCATE [TABLE] name` and the direct-parser `AUTO_INCREMENT` spelling for
   SQLite-compatible integer primary-key declarations.
9. Then move type descriptors into SQLite column metadata and enforce MySQL
   conversion/range/length rules in SQLite's insert/update path. Implemented
   for the first signed integer, supported unsigned integer, `DOUBLE`,
   `VARCHAR`, `BINARY`, `VARBINARY`, text families, blob families, `DECIMAL`,
   `DATE`, `DATETIME`, and `TIME` subset,
   with MyLite's public write paths now loading descriptors from the catalog
   before preparing physical writes.
10. Add VDBE statement-completion state for MySQL session functions.
    Implemented for native `ROW_COUNT()` and `LAST_INSERT_ID()` over the
    current direct CRUD subset, including generated `AUTO_INCREMENT` ids,
    result-producing `SELECT`, and direct `TRUNCATE` row-count behavior.

## Lemon Grammar Direction

These snippets describe the intended fork grammar shape. The exact SQLite
source edits will use SQLite's grammar names, but MyLite should keep the
compatibility intent clear:

```lemon
cmd ::= TRUNCATE opt_table nm.
opt_table ::= .
opt_table ::= TABLE.

column_constraint ::= AUTO_INCREMENT.
column_constraint ::= COMMENT string_literal.
column_constraint ::= CHARACTER SET charset_name.
column_constraint ::= COLLATE collation_name.

table_option ::= ENGINE eq_opt nm.
table_option ::= DEFAULT CHARSET eq_opt charset_name.
table_option ::= DEFAULT CHARACTER SET eq_opt charset_name.
table_option ::= DEFAULT COLLATE eq_opt collation_name.
table_option ::= COMMENT eq_opt string_literal.
table_option ::= AUTO_INCREMENT eq_opt integer_literal.

table_constraint ::= KEY nm_opt LP indexed_column_list RP index_options.
table_constraint ::= UNIQUE KEY nm_opt LP indexed_column_list RP index_options.
```

## Known Risks

- Full Unicode collation fidelity can dominate binary size if solved with a
  large dependency.
- MySQL unsigned 64-bit integers and exact decimal arithmetic do not fit
  SQLite's ordinary numeric storage classes.
- Patching SQLite's parser against the amalgamation is not maintainable by
  hand; the fork needs a source-tree refresh workflow that regenerates the
  amalgamation from reproducible inputs.
- The existing custom runtime and the fork path must not diverge in observed
  compatibility behavior. Shared MySQL runtime fixtures should become the
  contract between them.
