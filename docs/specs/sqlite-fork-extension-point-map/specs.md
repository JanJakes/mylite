# SQLite Fork Extension Point Map

## Status

This document records the current boundary between SQLite's existing extension
surface and MyLite-owned fork points. It is intentionally practical: use public
SQLite APIs where they preserve MySQL behavior and patch the source-tree fork
only where the public surface is too late, too narrow, or too expensive.

Implemented fork points:

- source-tree fork package with reproducible generated SQLite sources
- MyLite connection bootstrap for native collations, functions, and truncate
- MyLite connection bootstrap for SQLite-native foreign-key enforcement
- native SQLite collation registration used for MySQL comparison, ordering,
  grouping, duplicate elimination, and unique-key probes where generated SQL
  preserves expression collation
- MyLite column descriptors stored on SQLite `Column` objects
- owned per-column descriptor payloads for value-list types, including `ENUM`
  and `SET`
- VDBE write-time type checking through `OP_MyliteTypeCheck`
- VDBE read-time column transformation for MySQL types whose physical storage
  differs from displayed and numeric-context values, beginning with `ENUM` and
  `SET`
- binary string byte-length and fixed-length padding through column descriptors
- text/blob family byte-capacity checks and storage-class canonicalization
  through column descriptors
- decimal precision/scale rounding and range checks through column descriptors
- date, datetime, time, and year parsing, fractional rounding, mapping, and
  range checks through column descriptors
- timestamp parsing, fractional rounding, and strict range checks through a
  distinct column descriptor
- enum label/index assignment and readback through payload descriptors
- set label-list/bit-mask assignment and readback through payload descriptors
- bit-width assignment, fixed-width binary readback, and unsigned numeric
  context through column descriptors
- JSON column metadata and canonical text validation through column descriptors
  backed by SQLite's JSON parser
- update-mask-aware descriptor checking for SQLite `UPDATE` record creation
- structured fork diagnostics for MyLite-owned VDBE type-check failures
- structured fork diagnostics for native `NOT NULL`, `UNIQUE`, `PRIMARY KEY`,
  `CHECK`, and immediate foreign-key constraint failures
- structured fork warning publication from configured scalar callbacks,
  beginning with `IF()` condition conversion warnings
- connection-local fork condition-list storage with count and indexed-read APIs
  for statements that publish more than one condition
- top-level VDBE statement-start diagnostics reset for direct fork execution
- connection-local session state backed by SQLite client data, beginning with
  the default-schema value consumed by native `DATABASE()` and `SCHEMA()`
- connection-local VDBE completion state for native `ROW_COUNT()` and
  `LAST_INSERT_ID()`, including generated `AUTO_INCREMENT` rowid capture and
  MySQL's zero row count for direct `TRUNCATE`
- VDBE statement-time access for native MySQL current temporal functions,
  beginning with `NOW()`, `CURDATE()`, `CURTIME()`, and UTC/local synonyms
- parser admission for MySQL parenthesized current-time keyword forms,
  beginning with `CURRENT_DATE()`, `CURRENT_TIME([fsp])`, and
  `CURRENT_TIMESTAMP([fsp])`
- narrow parser admission for MySQL function names that SQLite tokenizes as
  syntax instead of identifiers, beginning with `ISNULL(expr)`
- tokenizer/parser admission for MySQL-only operators that SQLite rejects
  before expression lowering, beginning with scalar `<=>`
- direct SQLite parser admission for MySQL `TRUNCATE [TABLE] name`, with
  native row clearing plus `sqlite_sequence` reset for AUTOINCREMENT-backed
  tables
- SQLite keyword admission for MySQL `AUTO_INCREMENT` as an alias of the
  rowid-backed `AUTOINCREMENT` primitive in the current direct-parser subset
- direct SQLite parser admission for common MySQL `CREATE TABLE` table options,
  beginning with `ENGINE`, `DEFAULT CHARSET`, `DEFAULT CHARACTER SET`,
  `COLLATE`, `DEFAULT COLLATE`, `COMMENT`, and `AUTO_INCREMENT`
- native `sqlite_sequence` seeding from direct `CREATE TABLE ...
  AUTO_INCREMENT=N` for rowid-backed autoincrement tables
- direct SQLite parser and schema-builder admission for MySQL table-level
  `KEY`, `INDEX`, `UNIQUE KEY`, and `UNIQUE INDEX` declarations over simple
  column key parts, creating native SQLite indexes during `CREATE TABLE`
- direct SQLite parser and schema-builder admission for MySQL column-level
  `AUTO_INCREMENT`, including table-level primary-key promotion to native
  rowid autoincrement storage for integer-affinity single-column keys
- direct SQLite parser admission for MySQL `INSERT IGNORE`, lowered to
  SQLite's native conflict-ignore insert mode for the current direct DML subset
- public MyLite DML write-table loading that applies catalog descriptors before
  physical SQLite write statements are prepared
- public MyLite SELECT table loading that applies value-list descriptors before
  physical SQLite scan statements are prepared
- public MyLite SELECT table loading that applies scalar read descriptors,
  beginning with `BIT(M)`, before physical SQLite scan statements are prepared
- MyLite expression values with a MySQL numeric-context side channel for fork
  results such as `ENUM`, `SET`, and `BIT`, where direct display and arithmetic
  context intentionally differ
- public MyLite DML byte-length transport for binary string literals before
  SQLite placeholder binding
- materialized SELECT order-key comparison that uses descriptor numeric context
  for `ENUM`/`SET`/`BIT` while leaving generic string comparisons and aggregate
  extrema lexical

## Sources

- SQLite run-time loadable extensions:
  https://www.sqlite.org/loadext.html
- SQLite application-defined SQL functions:
  https://www.sqlite.org/c3ref/create_function.html
- SQLite application-defined collating sequences:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite virtual table interface:
  https://www.sqlite.org/vtab.html
- SQLite VFS interface:
  https://www.sqlite.org/vfs.html
- SQLite authorizer API:
  https://www.sqlite.org/c3ref/set_authorizer.html
- SQLite update/preupdate/commit hooks:
  https://www.sqlite.org/c3ref/update_hook.html
- Existing fork specs:
  `docs/specs/sqlite-source-tree-fork/specs.md`,
  `docs/specs/sqlite-fork-column-type-extension-points/specs.md`, and
  `docs/specs/sqlite-fork-type-coercion/specs.md`
- SQLite fork diagnostics bridge:
  `docs/specs/sqlite-fork-diagnostics-bridge/specs.md`
- SQLite fork binary string type descriptors:
  `docs/specs/sqlite-fork-binary-string-types/specs.md`
- SQLite fork text/blob family descriptors:
  `docs/specs/sqlite-fork-text-blob-family-descriptors/specs.md`
- SQLite fork decimal type descriptors:
  `docs/specs/sqlite-fork-decimal-type-descriptors/specs.md`
- SQLite fork temporal type descriptors:
  `docs/specs/sqlite-fork-temporal-type-descriptors/specs.md`
- SQLite fork YEAR type descriptors:
  `docs/specs/sqlite-fork-year-type-descriptors/specs.md`
- SQLite fork ENUM type descriptors:
  `docs/specs/sqlite-fork-enum-type-descriptors/specs.md`
- SQLite fork SET type descriptors:
  `docs/specs/sqlite-fork-set-type-descriptors/specs.md`
- SQLite fork BIT column descriptors:
  `docs/specs/sqlite-fork-bit-column-descriptors/specs.md`
- SQLite fork JSON column descriptors:
  `docs/specs/sqlite-fork-json-column-descriptors/specs.md`
- SQLite fork TIMESTAMP column descriptors:
  `docs/specs/sqlite-fork-timestamp-column-descriptors/specs.md`
- SQLite fork SELECT descriptor hydration:
  `docs/specs/sqlite-fork-select-descriptor-hydration/specs.md`
- SQLite fork constraint diagnostics:
  `docs/specs/sqlite-fork-constraint-diagnostics/specs.md`
- SQLite collation prefix uniqueness:
  `docs/specs/sqlite-collation-prefix-unique/specs.md`

This specification is independently authored from SQLite public documentation,
official MySQL 8.4 documentation already cited in the feature specs, observed
MySQL 8.4.9 fixtures, and the current MyLite codebase.

## Public SQLite Surface To Use

SQLite's public extension APIs are sufficient when MySQL compatibility can be
implemented as a named runtime primitive without changing parse trees, schema
objects, planner decisions, record construction, pager layout, or diagnostics
timing.

Use existing SQLite APIs for:

- Scalar, aggregate, and window functions whose arguments and result metadata
  can be represented by compact callbacks. Function names that SQLite tokenizes
  as operators or other syntax still need a small fork grammar rule before they
  can lower to those callbacks.
- Operators only when SQLite already tokenizes the syntax and its expression
  semantics can be expressed by existing parse nodes. MySQL-only operators such
  as `<=>` need fork tokenizer/parser admission before they can lower to a
  function or opcode.
- User-facing token changes must not be applied blindly to all SQLite parses.
  SQLite runs internally generated SQL for schema rewrites, including
  `ALTER TABLE`, and that SQL may rely on SQLite-native syntax such as `||`
  string concatenation. MySQL remapping for `||` therefore needs a user-SQL mode
  boundary or internal-parse escape hatch rather than a global tokenizer change.
- Collations where the comparison algorithm can be implemented directly and
  registered by MySQL collation name. MyLite-generated expressions must still
  carry the intended `COLLATE` clause when SQLite would otherwise treat a
  derived expression, such as `substr(column,1,n)`, as binary/default
  collation.
- Virtual tables for synthetic schemas such as `information_schema`,
  `performance_schema`, `sys`, and perhaps diagnostic views, provided metadata
  updates remain coordinated with MyLite's catalog.
- Connection client data for compact MyLite session state used by native
  callbacks, such as the selected default schema. This public SQLite surface is
  sufficient while the state is read by callbacks and owned by the connection;
  statement-completion state, warning lists, and row-identity allocation need
  fork points where SQLite completes VDBE execution. Implemented first for
  `ROW_COUNT()` and `LAST_INSERT_ID()`.
- Pure comparison helpers such as the first native `STRCMP()` slice when the
  result can be computed from the argument values plus the current supported
  collation subset. Broader collation coercibility, Unicode weights, and
  connection-collation selection need MyLite expression metadata rather than a
  bare callback alone.
- VFS or pager wrappers for opening MyLite files, temporary databases, and
  controlled file-system behavior when the on-disk SQLite database still starts
  at SQLite page 1.
- Authorizer, progress, busy, commit, rollback, update, and preupdate hooks for
  observation, statement interruption, and some policy checks.
- `sqlite3_trace_v2()` for instrumentation and test diagnostics.
- `sqlite3_db_config()` / `sqlite3_config()` for connection policy when SQLite
  already exposes the relevant switch, including enabling native foreign-key
  enforcement on configured MyLite connections.

These surfaces are not enough for core MySQL semantics that must be visible
while SQLite is generating bytecode or making storage decisions.

## Fork Points MyLite Needs

### Parser and schema builder

MySQL grammar cannot be made transparent through public SQLite callbacks. The
fork needs Lemon grammar support for common MySQL DDL and DML forms, then schema
builder changes that attach MyLite descriptors while tables and indexes are
created.

Required for:

- MySQL type syntax and attributes
- `AUTO_INCREMENT`, column comments, generated/invisible columns, and table
  options
- table-level MySQL index declarations, because public extension callbacks
  cannot admit inline `KEY` syntax or create schema objects while SQLite is
  building a table
- `INSERT ... SET`, `ON DUPLICATE KEY UPDATE`, `REPLACE`, `TRUNCATE`,
  `UPDATE ... JOIN`, and multi-table `DELETE`
- versioned comments and SQL-mode-sensitive parse decisions

### Column descriptors and assignment conversion

MySQL assignment conversion must run at SQLite's write boundary before records
are assembled. Public SQL functions can express a few conversions, but they
force every lowering path to generate custom wrapper SQL and do not cover direct
SQLite parser execution. The fork point is the SQLite table/column metadata and
VDBE record-build path.

Implemented first slice:

- descriptor API on SQLite schema columns
- `OP_MyliteTypeCheck` before `OP_MakeRecord`
- assignment-aware `UPDATE` checks using SQLite's changed-column map
- catalog-fed descriptors for public MyLite write paths
- catalog-fed value-list and `BIT` read descriptors for public MyLite
  table-backed read paths
- MyLite result materialization that preserves fork-provided display strings
  and numeric context for `ENUM`/`SET`

`ENUM` establishes a descriptor-payload and read-type boundary: physical
storage can be compact while selected values expose MySQL's string display and
numeric index behavior. `SET` reuses that payload ownership pattern with
bit-mask assignment and comma-list display semantics, including the 64th MySQL
member bit. MyLite's materialized SELECT layer now consumes that numeric
context for value-list order keys while preserving lexical behavior for
string-context comparisons and aggregate extrema. `BIT` confirms the same
read-type boundary is needed outside value-list descriptors: one stored integer
must expose fixed-width binary display bytes to string functions and unsigned
numeric context to arithmetic/order paths. `JSON` confirms a related
write-boundary need even when physical storage is currently text: assignment
validation must happen before SQLite affinity and record construction, while
metadata and diagnostics remain MySQL-owned. `TIMESTAMP` confirms that a
descriptor can share parser/formatter code with `DATETIME` while still carrying
separate range and future time-zone semantics.

The next type-adjacent fork points are accepted-assignment warnings,
SQL-mode-sensitive zero date handling, `TIME_TRUNCATE_FRACTIONAL`,
`TIMESTAMP` time-zone conversion, `YEAR(4)` declaration warnings, direct
SQLite parser/catalog descriptor loading, and broader scalar read-descriptor
hydration after schema reload. The next decimal-specific fork points are
comparison/index ordering and direct SQLite parser numeric-literal
preservation. The next JSON-specific fork points are binary JSON
normalization/storage, exact diagnostic messages with parser positions, JSON
comparison/index semantics, mutator/partial-update storage metadata, and direct
SQLite parser/catalog descriptor loading. The expression side still needs
continued MySQL numeric-context coercion work for collation-aware value-list
string comparisons, unsigned 64-bit bit rendering, cross-column descriptor
comparisons, and optimizer/index ordering.

### Diagnostics and warnings

SQLite can return errors, but MySQL compatibility needs condition codes,
SQLSTATEs, warning demotion, `IGNORE`, strict/non-strict behavior, affected row
accounting, and statement atomicity that line up with MySQL. Some can stay in
MyLite's statement layer, but direct fork execution needs a diagnostics bridge
from VDBE opcodes and functions into a MyLite connection diagnostics area.

Required fork direction:

- conversion opcodes report structured MySQL conditions, not only SQLite text;
  implemented first for `OP_MyliteTypeCheck`
- native constraint halts report structured MySQL conditions where SQLite's
  constraint class maps cleanly to MySQL; implemented first for `NOT NULL`,
  `UNIQUE`, `PRIMARY KEY`, and `CHECK`
- foreign-key counter bytecode carries child-side or parent-side context so the
  statement-level immediate FK error can publish MySQL's `1452` or `1451`
- scalar callbacks can publish successful-statement warnings through the fork
  diagnostics bridge; implemented first with an indexed connection-local
  condition list for statements that publish more than one warning
- top-level VDBE statement start clears the fork diagnostics area, giving
  direct fork execution current-statement warning visibility instead of stale
  records
- statement-stable functions such as `NOW()` need a VDBE statement timestamp
  or equivalent statement context. Implemented first by exposing SQLite's
  statement clock to MyLite callbacks for native current temporal functions.
- `IGNORE` and non-strict SQL modes can demote selected write errors to warnings
- VDBE statement completion can expose MySQL affected-row and warning metadata
- future diagnostics work needs exact condition messages, row/column metadata,
  and lifecycle exceptions for `SHOW WARNINGS`, `GET DIAGNOSTICS`, and public
  MyLite compound statements

### Statement and session completion state

Some MySQL information functions can be registered through
`sqlite3_create_function_v2()`, but the values they expose are not computable
inside the callback alone. `ROW_COUNT()` depends on the previous completed
statement, result-producing statements must publish `-1` only after their
result set is drained, write statements must publish the post-rollback affected
row count, and `TRUNCATE` must publish zero even when SQLite internally deletes
rows. `LAST_INSERT_ID()` depends on the first generated auto-increment value of
the previous successful statement, not merely the last rowid SQLite touched.

The implemented fork point stores compact MyLite session fields on `sqlite3`
and statement-local generated-id flags on `Vdbe`. `OP_NewRowid` records the
first generated `AUTOINCREMENT` rowid for a statement, and `sqlite3VdbeHalt()`
publishes `ROW_COUNT()` and `LAST_INSERT_ID()` only after SQLite knows whether
the statement succeeded, rolled back, read rows, or ran as direct `TRUNCATE`.
SQLite schema-initialization VDBEs are explicitly ignored so internal metadata
loads do not alter MySQL-visible session state. The public MyLite runtime keeps
its existing `last_insert_id` and `previous_row_count` fields synchronized with
the fork state while broader MySQL auto-increment allocation remains in the
current public layer.

The same boundary applies to current temporal functions. The callback
registration surface is sufficient for names and formatting, but the callback
needs a fork API to read SQLite's VDBE statement clock. The implemented slice
exposes that clock as Unix milliseconds and uses it for native `NOW()`,
`CURRENT_TIMESTAMP()`, `LOCALTIME()`, `LOCALTIMESTAMP()`, `UTC_TIMESTAMP()`,
`CURDATE()`, `CURRENT_DATE()`, `UTC_DATE()`, `CURTIME()`, `CURRENT_TIME()`, and
`UTC_TIME()`. This keeps all invocations in one statement stable while leaving
session time-zone conversion and microsecond-resolution clock capture for later
temporal work.

This also requires a grammar hook for current-time keyword names. SQLite can
parse bare `CURRENT_TIMESTAMP`, but a public extension cannot make
`CURRENT_TIMESTAMP(6)` legal once the tokenizer has emitted `CTIME_KW` instead
of a normal identifier token. The fork now admits the MySQL parenthesized forms
for `CURRENT_DATE`, `CURRENT_TIME`, and `CURRENT_TIMESTAMP` and lowers them to
ordinary function expressions.

### File format and pager

The `.mylite` single-file format should remain portable. If MyLite reserves a
header before the SQLite database image or stores sidecar metadata inside the
same file, a public VFS alone may not be enough because SQLite's pager assumes
page 1 begins at byte 0. The fork may need pager offset support with careful
testing of WAL, journal, backup, vacuum, and corruption paths.

### Auto-increment and row identity

SQLite rowid is the right primitive for common single-column integer primary
keys, but MySQL-visible semantics exceed ordinary rowid behavior:

- all MySQL integer families can be `AUTO_INCREMENT`
- `CREATE TABLE ... AUTO_INCREMENT=N` must seed the next generated value;
  implemented first for direct parser rowid-backed `AUTOINCREMENT` tables by
  writing `sqlite_sequence` during table creation
- explicit values advance the next value differently from SQLite sequence rules
- failed multi-row statements can leave gaps
- `NO_AUTO_VALUE_ON_ZERO` changes zero handling
- unsigned `BIGINT` exceeds signed rowid range

Current public MyLite code manages much of this above SQLite. The fork should
eventually move allocation and sequence metadata closer to the write path.

### Metadata and virtual schemas

Virtual tables can expose synthetic schemas, but DDL must update the metadata
that backs them. The fork should avoid a second catalog engine. The long-term
shape is SQLite schema objects plus compact MyLite metadata fields that drive
`SHOW`, `INFORMATION_SCHEMA`, result-set metadata, and protocol descriptors.

### Collation propagation

SQLite's collation extension surface is strong enough for native comparison
once a `CollSeq` reaches the expression or index key. The fork does not need a
new VDBE comparison opcode for the current ASCII-oriented supported registry.
The MyLite lowering layer does need to preserve collation on generated
expressions used for MySQL prefix indexes, duplicate probes, and existing-row
unique validation. Implemented first for `INSERT`, duplicate update, `UPDATE`,
`CREATE UNIQUE INDEX`, and `ALTER TABLE ... ADD UNIQUE` prefix checks.

## Current Technical Call

For the next foundation work, prefer this order:

1. Keep using public SQLite APIs for functions, collations, instrumentation,
   and synthetic schema exposure.
2. Extend the column-descriptor/VDBE assignment path before adding more
   statement-specific DML lowering code.
3. Add direct parser/schema-builder attachment and schema reload hydration only
   after catalog-fed descriptors cover the semantic primitive.
4. Add fork diagnostics only when a conversion or constraint behavior needs
   MySQL warning/error semantics that cannot be represented by SQLite errors.
5. Add narrow tokenizer and grammar hooks when MySQL-compatible syntax is
   blocked before the public extension registry is consulted, but separate
   user SQL from SQLite internal SQL before changing meanings of existing
   SQLite tokens such as `||`.
6. Move row identity, sequence allocation, and constraint mapping into the fork
   when public MyLite SQL lowering cannot preserve MySQL affected-row,
   duplicate-key, or auto-increment side effects without duplicated logic.
7. Delay broad parser patches until the primitive semantics are strong enough
   that parser integration can attach metadata instead of recreating behavior.
