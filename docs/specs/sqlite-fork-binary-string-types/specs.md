# SQLite Fork Binary String Type Descriptors

## Status

This slice extends the SQLite-fork column descriptor mechanism with the first
binary string assignment types:

- `BINARY(N)` byte-length validation and zero-byte right padding
- `VARBINARY(N)` byte-length validation without padding
- structured MySQL condition reporting for strict over-length assignment
- public MyLite DML descriptor loading from cataloged `BINARY` and
  `VARBINARY` columns
- direct SQLite descriptor tests and public MyLite CRUD tests covering
  `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`

Deferred scope:

- `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, and `LONGBLOB` capacity checks
- complete binary-string comparison and collation behavior in all SQLite
  planner paths
- exact MySQL message text with schema, table, and row interpolation
- non-strict and `IGNORE` truncation warnings
- parser/catalog reload inside SQLite without MyLite's current catalog bridge

## Sources

- MySQL 8.4 Reference Manual, The `BINARY` and `VARBINARY` Types:
  https://dev.mysql.com/doc/refman/8.4/en/binary-varbinary.html
- MySQL 8.4 Reference Manual, String Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- SQLite dynamic type system:
  https://www.sqlite.org/datatype3.html
- SQLite VDBE fork descriptor specs:
  `docs/specs/sqlite-fork-column-type-extension-points/specs.md`,
  `docs/specs/sqlite-fork-type-coercion/specs.md`, and
  `docs/specs/sqlite-fork-diagnostics-bridge/specs.md`

This specification is independently authored from official documentation,
observed MySQL 8.4.9 runtime behavior, and the current MyLite codebase.

## MySQL 8.4.9 Behavior Baseline

Runtime probes were executed on 2026-05-06 against the official
`mysql:8.4.9` Docker image in container `mylite-mysql-849`, using MySQL's
default strict SQL mode.

For `mysql-binary-string-coercion.sql`, MySQL produced:

```text
after-insert	1	610000	3	C3A9	2
after-insert	2	363500	3	31323334	4
after-update	1	610000	3	C3A9	2
after-update	2	787900	3	7A	1
after-duplicate-update	1	610000	3	C3A9	2
after-duplicate-update	2	757600	3	7778	2
after-replace	1	610000	3	C3A9	2
after-replace	2	757600	3	7778	2
after-replace	3	720000	3	7374	2
```

Strict over-length probes produced:

| Statement shape | MySQL 8.4.9 result |
| --- | --- |
| `BINARY(3)` assignment of four bytes | error 1406, SQLSTATE `22001` |
| `VARBINARY(4)` assignment of five bytes | error 1406, SQLSTATE `22001` |

## Runtime Design

Binary string assignment belongs in the same SQLite fork write boundary as
integer, `DOUBLE`, and `VARCHAR` assignment. SQL wrapper functions cannot cover
all insert/update bytecode paths, cannot catch direct SQLite writes through the
fork, and make fixed-length padding a per-statement lowering concern.

MyLite attaches either a `BINARY` or `VARBINARY` descriptor to SQLite's
in-memory `Column` object before preparing writes. SQLite then emits
`OP_MyliteTypeCheck` before `OP_MakeRecord`.

The VDBE conversion rules for this slice are:

- `NULL` remains `NULL` so ordinary `NOT NULL` enforcement owns nullability.
- Existing text and blob values are measured by bytes.
- Numeric values are first converted to their string representation, matching
  the already implemented assignment path for `VARCHAR`.
- `VARBINARY(N)` rejects values longer than `N` bytes.
- `BINARY(N)` rejects values longer than `N` bytes and pads shorter values with
  zero bytes to exactly `N` bytes.
- Stored binary values use SQLite BLOB storage so byte length, embedded zero
  bytes, and direct `hex()`/`length()` inspection remain stable.

Over-length assignment publishes MySQL condition 1406 with SQLSTATE `22001`
through the fork diagnostics bridge before returning the SQLite execution
error.

## Existing SQLite Extension Surface

SQLite public APIs can expose binary scalar functions and collations, but they
are not sufficient for this assignment behavior:

- loadable functions would require every physical write lowering path to wrap
  every assigned value;
- hooks fire after record construction or do not allow mutation of the new
  row value;
- collations do not enforce declared byte capacity or fixed-length padding;
- CHECK constraints can reject length but cannot coerce numeric values or pad
  `BINARY(N)` without duplicating the column expression in generated SQL.

The descriptor/VDBE fork point is therefore the right integration point for
native MySQL binary string assignment.

## Lemon Grammar Direction

No new grammar is introduced in this slice. MyLite already parses `BINARY(N)`,
`VARBINARY(N)`, and character-string aliases that resolve to binary string
catalog descriptors. The future SQLite parser fork should attach the same
descriptor directly while building the SQLite schema object.

## Tests

Executable coverage must include:

- direct SQLite descriptor `INSERT` and `UPDATE` with text, blob, and numeric
  inputs
- direct SQLite strict over-length failures and fork condition publication
- public MyLite `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`
  matching the MySQL fixture
- public MyLite strict over-length failures for both fixed and variable binary
  strings
- MySQL fixture diff against
  `mysql-binary-string-coercion.expected.tsv`

## Compatibility Status

This feature is `🟡`: the native write-path primitive is implemented for
`BINARY` and `VARBINARY`, but full binary-string semantics across comparisons,
blob families, non-strict modes, and direct SQLite parser cataloging remain
future work.
