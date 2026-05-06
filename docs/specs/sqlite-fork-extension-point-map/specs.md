# SQLite Fork Extension Point Map

## Status

This document records the current boundary between SQLite's existing extension
surface and MyLite-owned fork points. It is intentionally practical: use public
SQLite APIs where they preserve MySQL behavior and patch the source-tree fork
only where the public surface is too late, too narrow, or too expensive.

Implemented fork points:

- source-tree fork package with reproducible generated SQLite sources
- MyLite connection bootstrap for native collations, functions, and truncate
- native SQLite collation registration used for MySQL comparison, ordering,
  grouping, duplicate elimination, and unique-key probes where generated SQL
  preserves expression collation
- MyLite column descriptors stored on SQLite `Column` objects
- VDBE write-time type checking through `OP_MyliteTypeCheck`
- binary string byte-length and fixed-length padding through column descriptors
- decimal precision/scale rounding and range checks through column descriptors
- date and datetime parsing, fractional rounding, and range checks through
  column descriptors
- update-mask-aware descriptor checking for SQLite `UPDATE` record creation
- structured fork diagnostics for MyLite-owned VDBE type-check failures
- public MyLite DML write-table loading that applies catalog descriptors before
  physical SQLite write statements are prepared

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
- SQLite fork decimal type descriptors:
  `docs/specs/sqlite-fork-decimal-type-descriptors/specs.md`
- SQLite fork temporal type descriptors:
  `docs/specs/sqlite-fork-temporal-type-descriptors/specs.md`
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
  can be represented by compact callbacks.
- Collations where the comparison algorithm can be implemented directly and
  registered by MySQL collation name. MyLite-generated expressions must still
  carry the intended `COLLATE` clause when SQLite would otherwise treat a
  derived expression, such as `substr(column,1,n)`, as binary/default
  collation.
- Virtual tables for synthetic schemas such as `information_schema`,
  `performance_schema`, `sys`, and perhaps diagnostic views, provided metadata
  updates remain coordinated with MyLite's catalog.
- VFS or pager wrappers for opening MyLite files, temporary databases, and
  controlled file-system behavior when the on-disk SQLite database still starts
  at SQLite page 1.
- Authorizer, progress, busy, commit, rollback, update, and preupdate hooks for
  observation, statement interruption, and some policy checks.
- `sqlite3_trace_v2()` for instrumentation and test diagnostics.
- `sqlite3_db_config()` / `sqlite3_config()` for connection policy when SQLite
  already exposes the relevant switch.

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

Next likely descriptor families:

- `TIME`, `TIMESTAMP`, and `YEAR` temporal values with SQL-mode behavior
- blob-family capacity checks
- `ENUM`, `SET`, `JSON`, and bit values

The next temporal-specific fork points are accepted-assignment warnings,
SQL-mode-sensitive zero date handling, `TIMESTAMP` time-zone conversion, and
direct SQLite parser/catalog descriptor loading. The next decimal-specific fork
points are comparison/index ordering and direct SQLite parser numeric-literal
preservation.

### Diagnostics and warnings

SQLite can return errors, but MySQL compatibility needs condition codes,
SQLSTATEs, warning demotion, `IGNORE`, strict/non-strict behavior, affected row
accounting, and statement atomicity that line up with MySQL. Some can stay in
MyLite's statement layer, but direct fork execution needs a diagnostics bridge
from VDBE opcodes and functions into a MyLite connection diagnostics area.

Required fork direction:

- conversion opcodes report structured MySQL conditions, not only SQLite text;
  implemented first for `OP_MyliteTypeCheck`
- `IGNORE` and non-strict SQL modes can demote selected write errors to warnings
- VDBE statement completion can expose MySQL affected-row and warning metadata

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
3. Add fork diagnostics only when a conversion or constraint behavior needs
   MySQL warning/error semantics that cannot be represented by SQLite errors.
4. Delay broad parser patches until the primitive semantics are strong enough
   that parser integration can attach metadata instead of recreating behavior.
