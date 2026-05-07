# SQLite Fork SELECT Descriptor Hydration

## Status

Implemented for the first read-visible scalar descriptor slice. The initial
scope rehydrates `BIT(M)` descriptors during public MyLite table-backed
`SELECT` catalog loading. Existing `ENUM` and `SET` value-list descriptor
hydration remains in place.

## References

- MySQL 8.4 Reference Manual, Bit-Value Type - BIT:
  https://dev.mysql.com/doc/en/bit-type.html
- MySQL 8.4 Reference Manual, Bit-Value Literals:
  https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html
- SQLite application-defined SQL functions:
  https://www.sqlite.org/c3ref/create_function.html
- SQLite application-defined collating sequences:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite virtual table interface:
  https://www.sqlite.org/vtab.html
- SQLite authorizer API:
  https://www.sqlite.org/c3ref/set_authorizer.html
- Existing SQLite fork column descriptor specs:
  `docs/specs/sqlite-fork-column-type-extension-points/specs.md` and
  `docs/specs/sqlite-fork-bit-column-descriptors/specs.md`
- MySQL 8.4.9 runtime expectations from
  `docs/specs/sqlite-fork-bit-column-descriptors/mysql-bit-column-crud.sql`

This specification is independently authored from official MySQL and SQLite
documentation, observed MySQL 8.4.9 behavior, and the current MyLite codebase.
It does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
other restrictively licensed implementation sources.

## Scope

Public MyLite currently stores MySQL column semantics in its own catalog and
attaches fork descriptors to live SQLite `Column` objects before preparing
write statements. That makes assignment conversion native in the fork, but it
does not by itself make descriptors durable across connection reopen: SQLite's
schema objects are rebuilt from SQLite's own schema text, not from MyLite's
catalog.

This slice adds descriptor hydration for read-visible MySQL types when MyLite
loads table metadata for a public table-backed `SELECT`. It covers:

- `ENUM` and `SET` through the existing value-list descriptor loader;
- `BIT(M)` through a new scalar read-descriptor loader;
- preservation of MySQL `BIT` string display and numeric context after a
  database file is closed and reopened through MyLite;
- ordinary public MyLite catalog loading, without direct SQLite parser changes.

Out of scope for this slice:

- direct SQLite parser support for MySQL column syntax;
- direct SQLite schema reload from SQLite's catalog without MyLite catalog
  participation;
- SELECT-time attachment of write-only scalar descriptors that do not yet have
  read-time semantics;
- migration of descriptor metadata into SQLite's persisted schema format;
- non-strict warning demotion or SQL-mode-specific read behavior.

## Existing SQLite Extension Surface

The public SQLite extension surface can cover many MySQL-facing primitives:

- functions implement scalar and aggregate behavior;
- collations implement named comparison rules once generated SQL preserves the
  intended collation;
- virtual tables can expose metadata views;
- authorizer and trace hooks can observe or block operations.

Those APIs cannot transparently solve this problem. `BIT(M)` readback needs the
SQLite bytecode generated for a table column to know that one stored integer
must expose fixed-width binary bytes in string context and an unsigned numeric
value in arithmetic/order context. A SQL function wrapper would require every
lowering path to wrap every column reference and would still miss direct table
column reads. A virtual table would replace storage instead of integrating with
SQLite's ordinary b-tree, planner, transaction, and indexing machinery. The
correct boundary is still the fork's column descriptor plus VDBE read opcode.

## Design

MyLite owns durable MySQL metadata. SQLite owns transient schema objects and
bytecode generation. Whenever MyLite resolves a public table for a table-backed
`SELECT`, the catalog loader must attach all read-visible fork descriptors
needed by the physical scan before preparing SQL that reads from the table.

For the first scalar slice:

- if `DATA_TYPE = 'bit'` and `NUMERIC_PRECISION` is present, attach a
  `MYLITE_SQLITE_FORK_COLUMN_TYPE_BIT` descriptor to the physical SQLite
  column;
- store the declared width in the descriptor's numeric precision field;
- leave columns without read-time fork behavior unchanged;
- keep write-time descriptor loading in the DML write-table path, where SQL
  mode and assignment details are already available.

No grammar changes are required for this feature. The relevant MyLite Lemon
syntax for the already-supported type remains:

```lemon
column_type ::= bit_column_type.
bit_column_type ::= BIT opt_column_length.
```

## Runtime Semantics

After reopening a `.mylite` file through public MyLite:

- table-backed `SELECT bit_col` exposes fixed-width binary bytes;
- `LENGTH(bit_col)` returns `ceil(M / 8)`;
- `BIT_LENGTH(bit_col)` returns `8 * ceil(M / 8)`;
- `bit_col + 0` returns the stored numeric value for values currently covered
  by MyLite's signed 64-bit numeric-context path;
- ordering over materialized `BIT` values continues to use numeric context in
  the current MyLite SELECT runtime.

The implementation intentionally hydrates descriptors before preparing the
physical SQLite scan. Prepared statements keep the descriptors that were
present during code generation; later descriptor changes do not retroactively
change already-prepared statements.

## Fixture

The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-bit-column-descriptors/mysql-bit-column-crud.sql`
already verifies the read semantics that must survive reopen:

- `b'101010101'` in `BIT(9)` has numeric value `341`;
- `LENGTH()` returns `2`;
- `BIT_LENGTH()` returns `16`.

The MyLite runtime test for this slice creates a WordPress-like feature flag
table in a file-backed database, inserts a `BIT(9)` value, closes the database,
reopens it, and verifies that the same read semantics remain available before
any subsequent write-table descriptor loading occurs.

## Compatibility Status

MyLite now rehydrates public table-backed SELECT descriptors for value-list
types and `BIT(M)` after connection reopen. Direct SQLite parser/catalog
descriptor reload, persisted SQLite-schema descriptor metadata, and scalar
descriptor hydration for future read-visible types remain deferred.
