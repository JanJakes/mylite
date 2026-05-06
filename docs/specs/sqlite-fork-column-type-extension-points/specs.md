# SQLite Fork Column Type Extension Points

## Status

This slice adds the first real MyLite semantic extension point inside the
SQLite source-tree fork. It does not change SQLite's SQL grammar yet. Instead,
MyLite can attach MySQL column type descriptors to SQLite's in-memory schema
objects after table creation or schema load, and SQLite emits a native VDBE
assignment-coercion opcode when preparing writes to those tables.

Implemented scope:

- add MyLite-owned type metadata to SQLite's internal `Column` objects
- add owned descriptor payload storage for column types with value lists
- add `mylite_sqlite_fork_set_column_type()` and
  `mylite_sqlite_fork_clear_column_type()` for attaching descriptors to a live
  SQLite schema
- add `mylite_sqlite_fork_set_enum_column_type()` for attaching copied
  `ENUM` value-list descriptors to a live SQLite schema
- add `OP_MyliteTypeCheck`, emitted from SQLite's `sqlite3TableAffinity()` when
  a target table has MyLite descriptors
- add `OP_MyliteColumnReadType` for direct column reads whose displayed and
  numeric-context values differ from physical storage
- support strict assignment coercion for signed integer ranges, supported
  unsigned integer ranges, `DOUBLE`, `VARCHAR(n)`, `BINARY(n)`,
  `VARBINARY(n)`, text families, blob families, `DECIMAL(p,s)`, `DATE`,
  `DATETIME(fsp)`, `TIME(fsp)`, `YEAR`, and `ENUM(...)`
- preserve ordinary SQLite affinity for columns without a MyLite descriptor
- cover direct SQLite `INSERT` and `UPDATE` statements that write through the
  new descriptor path without SQL wrapper functions
- make `UPDATE` descriptor checks assignment-aware by using SQLite's
  changed-column mask instead of rechecking the whole row
- publish structured MySQL condition codes for descriptor-owned VDBE failures
  through the fork diagnostics bridge
- load MyLite catalog column metadata into those descriptors before public
  MyLite write statements prepare their physical SQLite `INSERT`, `UPDATE`,
  `REPLACE`, and duplicate-key update SQL

Deferred scope:

- direct SQLite parser support for MySQL column type syntax
- direct SQLite parser/catalog descriptor reload for MySQL SQL executed without
  MyLite's current statement layer
- full unsigned `BIGINT` values above SQLite's signed 64-bit integer storage
  range
- exact MySQL diagnostic messages, row interpolation, complete warning records,
  and `IGNORE` demotion
- non-strict SQL mode clipping and string truncation behavior
- `TIMESTAMP`, JSON, `SET`, bit, and spatial assignment conversion
- preserving MyLite descriptors through SQLite-native schema rebuilds that are
  not coordinated by MyLite

## Sources

- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  `https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html`
- MySQL 8.4 Reference Manual, Out-of-Range and Overflow Handling:
  `https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html`
- MySQL 8.4 Reference Manual, Numeric Type Syntax:
  `https://dev.mysql.com/doc/refman/8.4/en/numeric-types.html`
- MySQL 8.4 Reference Manual, String Type Syntax:
  `https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html`
- SQLite Datatypes and type affinity:
  `https://www.sqlite.org/datatype3.html`
- Existing SQLite fork type coercion foundation:
  `docs/specs/sqlite-fork-type-coercion/specs.md`
- SQLite fork binary string type descriptors:
  `docs/specs/sqlite-fork-binary-string-types/specs.md`
- SQLite fork decimal type descriptors:
  `docs/specs/sqlite-fork-decimal-type-descriptors/specs.md`
- SQLite fork temporal type descriptors:
  `docs/specs/sqlite-fork-temporal-type-descriptors/specs.md`
- SQLite fork time type descriptors:
  `docs/specs/sqlite-fork-time-type-descriptors/specs.md`
- SQLite fork text/blob family descriptors:
  `docs/specs/sqlite-fork-text-blob-family-descriptors/specs.md`
- SQLite fork year type descriptors:
  `docs/specs/sqlite-fork-year-type-descriptors/specs.md`
- SQLite fork enum type descriptors:
  `docs/specs/sqlite-fork-enum-type-descriptors/specs.md`
- Existing SQLite source-tree fork package:
  `docs/specs/sqlite-source-tree-fork/specs.md`

This specification is independently authored from official MySQL and SQLite
documentation, observed MySQL 8.4.9 runtime behavior captured by the existing
type-coercion fixture, and the current MyLite codebase. It does not copy MySQL,
MariaDB, Percona, or other restrictively licensed implementation sources.

## Design

The extension point is intentionally lower than SQL functions and higher than
the SQLite record format. The MyLite descriptor is attached to SQLite's parsed
`Table` / `Column` schema objects, then SQLite code generation sees that table
flag and emits a native type-check opcode before `OP_MakeRecord`.

This is the right first fork boundary because:

- It keeps SQLite's planner, row storage, indexes, transactions, CTEs, triggers,
  and window-function machinery available.
- It avoids rewriting assignment conversion in every MyLite DML lowering path.
- It keeps MySQL type metadata out of SQLite's public file format until MyLite's
  catalog-reload rules are designed.
- It lets MyLite incrementally replace SQL wrapper functions with native VDBE
  behavior.

The public fork API takes SQLite return codes. `SQLITE_OK` means the descriptor
was attached. `SQLITE_MISUSE` reports invalid API arguments or invalid
descriptor parameters. `SQLITE_NOTFOUND` reports a missing table or column.

Descriptors are connection-local in this slice. MyLite now attaches them while
loading a write table from its catalog, before preparing physical SQLite write
statements. Prepared statements keep the generated bytecode they already have;
changing a descriptor after preparation does not retroactively change existing
statements.

## Runtime Semantics

`OP_MyliteTypeCheck` receives the same table object and register range that
SQLite already uses for table affinity. For each target column:

- If there is no MyLite descriptor, ordinary SQLite affinity is applied.
- `NULL` is passed through unchanged; physical `NOT NULL` constraints and
  higher-level MyLite diagnostics remain responsible for nullability.
- Signed integer descriptors coerce finite numeric values using MySQL-observed
  half-away-from-zero assignment rounding, then enforce the configured signed
  range.
- Supported unsigned integer descriptors reject negative values and values above
  the configured non-negative maximum.
- `DOUBLE` descriptors coerce finite numeric values to a SQLite real value.
- `VARCHAR(n)` descriptors convert through SQLite's UTF-8 text representation
  and enforce a UTF-8 character-count maximum.
- `BINARY(n)` descriptors convert non-binary values through SQLite's UTF-8
  text representation, reject values above the declared byte length, store the
  value as a BLOB, and right-pad shorter values with zero bytes.
- `VARBINARY(n)` descriptors convert non-binary values through SQLite's UTF-8
  text representation, reject values above the declared byte length, and store
  the value as a BLOB without padding.
- Text-family descriptors convert values to UTF-8 text, reject values above
  the cataloged byte capacity, and keep SQLite TEXT storage.
- Blob-family descriptors convert numeric values through SQLite's UTF-8 text
  representation, reject values above the cataloged byte capacity, and store
  the value as a BLOB without padding.
- `DECIMAL(p,s)` descriptors parse exact decimal text when available, round
  half away from zero to the declared scale, reject post-round range overflow,
  reject negative values for unsigned decimal columns, and store canonical
  fixed-scale text.
- `DATE` descriptors validate supported date and datetime-shaped inputs,
  reject invalid dates under the default strict SQL mode baseline, apply
  fractional carry where the input includes a time portion, and store canonical
  `YYYY-MM-DD` text. The opt-in `ALLOW_ZERO_TEMPORAL` descriptor flag accepts
  only the all-zero date sentinel for statement paths that require MySQL
  warning-style zero insertion before full SQL-mode warning demotion exists.
- `DATETIME(fsp)` descriptors validate supported date/time inputs, round
  fractional seconds to the declared precision, reject invalid datetimes and
  post-round overflow, and store canonical text with the declared fractional
  scale. `ALLOW_ZERO_TEMPORAL` likewise accepts only the all-zero datetime
  sentinel.
- `TIME(fsp)` descriptors validate supported elapsed-time inputs, including
  negative values, day-plus-time strings, colon abbreviations, and compact
  numeric/text forms; round fractional seconds to the declared precision;
  reject malformed or out-of-range values; canonicalize negative zero; and
  store canonical text.
- `YEAR` descriptors distinguish numeric zero from quoted zero at the
  assignment boundary, map one- and two-digit years through MySQL's
  `2001..2069` and `1970..1999` windows, round fractional inputs before
  mapping, reject values outside `1901..2155` plus the `0000` sentinel, and
  store canonical four-character text.
- `ENUM` descriptors store copied value-list metadata on the column, convert
  labels and numeric indexes to compact one-based integer storage, reject
  invalid strict assignments with condition 1265 / SQLSTATE `01000`, and
  use read-time metadata to display labels while preserving numeric index
  behavior for arithmetic contexts.

On failure, SQLite aborts the statement with `SQLITE_CONSTRAINT_DATATYPE` and a
message naming the failed conversion and target column. The fork diagnostics
bridge exposes the first MySQL condition codes and SQLSTATE values for these
failures, while exact MySQL text and warning demotion remain later work.

For `UPDATE`, descriptor checks are emitted only for logical columns marked as
changed by SQLite's update planner. Unchanged columns still participate in the
new record and indexes, but they are not revalidated by MyLite type descriptors.
This matters after schema changes and legacy imports, where existing stored
values may not be valid assignments for the current MySQL-visible type.

## Grammar Direction

No grammar change is introduced in this slice.

The future SQLite parser patch should let MySQL column type syntax attach the
same descriptor directly while building the table. Until then, MyLite's own
parser and schema catalog remain authoritative, and the fork API is the bridge
between MyLite metadata and native SQLite execution.

## Tests

The executable tests must cover:

- source-tree SQLite still builds and reports the pinned version
- MyLite fork primitives still register on a SQLite connection
- a table can be annotated with signed integer, unsigned integer, `DOUBLE`,
  `VARCHAR`, `BINARY`, `VARBINARY`, text-family, blob-family, `DECIMAL`,
  `DATE`, `DATETIME`, `TIME`, and `YEAR` descriptors
- direct SQLite `INSERT` coerces numeric strings, numeric-to-text values, and
  approximate values through native descriptors
- direct SQLite `UPDATE` uses the same native descriptor path
- direct SQLite `UPDATE` checks assigned descriptor columns without revalidating
  unchanged legacy stored values
- out-of-range integer, negative unsigned integer, over-length `VARCHAR`, and
  invalid `DOUBLE`, over-length `BINARY`, and over-length `VARBINARY`
  assignments fail through the native opcode
- invalid decimal text, post-round decimal overflow, and unsigned-negative
  decimal assignments fail through the native opcode
- invalid date text, invalid datetime text, and post-round datetime overflow
  fail through the native opcode
- invalid and out-of-range time text fails through the native opcode
- invalid and out-of-range year values fail through the native opcode
- valid enum labels, numeric indexes, quoted integer indexes, numeric-looking
  labels, empty labels, selected labels, and numeric-context enum indexes pass
  through the native descriptor path
- invalid enum labels and indexes fail through the native opcode
- over-length text-family and blob-family assignments fail through the native
  opcode
- zero temporal sentinels are accepted only when the descriptor explicitly
  enables `ALLOW_ZERO_TEMPORAL`

The existing MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-type-coercion/mysql-basic-type-coercion.sql` remains
the runtime baseline for the first supported numeric and text type behavior.
The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-binary-string-types/mysql-binary-string-coercion.sql`
is the runtime baseline for the first supported binary string behavior.
The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-decimal-type-descriptors/mysql-decimal-coercion.sql`
is the runtime baseline for the first supported decimal assignment behavior.
The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-temporal-type-descriptors/mysql-temporal-coercion.sql`
is the runtime baseline for the first supported date/datetime assignment
behavior.
The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-time-type-descriptors/mysql-time-coercion.sql` is the
runtime baseline for the first supported `TIME(fsp)` assignment behavior.
The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-text-blob-family-descriptors/mysql-text-blob-family-coercion.sql`
is the runtime baseline for the first supported text/blob family assignment
behavior.
The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-year-type-descriptors/mysql-year-coercion.sql` is the
runtime baseline for the first supported `YEAR` assignment behavior.
The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-enum-type-descriptors/mysql-enum-coercion.sql` is the
runtime baseline for the first supported `ENUM` assignment and readback
behavior.

## Compatibility Status

This feature is `🟡` because MyLite now has a genuine SQLite-fork extension
point for write-time type semantics, but only a few basic types and strict-mode
failure paths are implemented.
