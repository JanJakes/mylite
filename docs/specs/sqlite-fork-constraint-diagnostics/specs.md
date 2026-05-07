# SQLite Fork Constraint Diagnostics

## Status

Implemented for the first native constraint diagnostics slice. The initial
scope maps SQLite-native `NOT NULL`, `UNIQUE`, and `PRIMARY KEY` constraint
failures into MySQL condition metadata exposed through the existing fork
diagnostics bridge.

## References

- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, INSERT Statement:
  https://dev.mysql.com/doc/en/insert.html
- MySQL 8.4 Error Message Reference:
  https://dev.mysql.com/doc/mysql-errors/8.4/en/
- SQLite ON CONFLICT clause:
  https://www.sqlite.org/lang_conflict.html
- SQLite result and extended result codes:
  https://www.sqlite.org/rescode.html
- Existing SQLite fork diagnostics bridge:
  `docs/specs/sqlite-fork-diagnostics-bridge/specs.md`

This specification is independently authored from official MySQL and SQLite
documentation, observed MySQL 8.4.9 behavior, and the current MyLite codebase.
It does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
other restrictively licensed implementation sources.

## Scope

SQLite already has efficient native constraint machinery for ordinary row
writes. MyLite should use that machinery where possible instead of prechecking
every condition in generated SQL. The missing fork primitive is diagnostics:
SQLite extended constraint codes and messages do not carry MySQL error numbers
or SQLSTATEs.

This slice covers the first common native constraint conditions:

- `NOT NULL` violations report MySQL error `1048`, SQLSTATE `23000`;
- `UNIQUE` violations report MySQL error `1062`, SQLSTATE `23000`;
- `PRIMARY KEY` violations report MySQL error `1062`, SQLSTATE `23000`;
- `INSERT` and `UPDATE` direct SQLite execution through the fork publish the
  same condition metadata when the native VDBE constraint halt executes;
- public MyLite error mapping can consume those conditions through the existing
  fork bridge when a physical SQLite write reaches native constraints.

Out of scope for this slice:

- exact MySQL duplicate-entry and not-null message text;
- warning demotion for `IGNORE` and non-strict modes;
- `CHECK` diagnostics, which require MySQL error `3819` and exact constraint
  naming policy;
- foreign-key diagnostics, where insert/update/delete need different MySQL
  error numbers and parent/child context;
- generated-column diagnostics and dependency-specific messages;
- direct MySQL parser support inside SQLite.

## Existing SQLite Extension Surface

Public SQLite hooks are not early enough for this behavior:

- application-defined functions and collations are not invoked for native
  constraint halts;
- authorizer hooks see statement access, not row-level constraint outcomes;
- update/preupdate hooks run around row changes and cannot replace a native
  VDBE halt's error metadata reliably;
- virtual tables can report custom constraint diagnostics, but replacing
  ordinary table storage with virtual tables would lose the planner, b-tree,
  transaction, and index behavior MyLite wants from SQLite.

The right fork point is the VDBE constraint halt path. SQLite already tags
constraint halt opcodes with the constraint family; MyLite can attach MySQL
condition metadata there without changing storage, query planning, or normal
SQLite error text yet.

## MySQL 8.4.9 Runtime Behavior

The verified MySQL 8.4.9 fixture lives in:

- `docs/specs/sqlite-fork-constraint-diagnostics/mysql-constraint-diagnostics.sql`
- `docs/specs/sqlite-fork-constraint-diagnostics/mysql-constraint-diagnostics.expected.stdout.tsv`
- `docs/specs/sqlite-fork-constraint-diagnostics/mysql-constraint-diagnostics.expected.stderr`

Observed strict-mode behavior:

- inserting or updating `NULL` into a `NOT NULL` column fails with
  `ERROR 1048 (23000)`;
- duplicate primary-key and duplicate unique-key writes fail with
  `ERROR 1062 (23000)`;
- failed rows do not change the stored result set.

The fixture was verified with:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --force \
  < docs/specs/sqlite-fork-constraint-diagnostics/mysql-constraint-diagnostics.sql
```

## Design

The existing fork diagnostics bridge stores one connection-local condition on
the SQLite handle. `OP_MyliteTypeCheck` already uses it for MyLite-owned
assignment conversions. This slice extends the same bridge to native SQLite
constraint halts:

- when `OP_Halt` or `OP_HaltIfNull` exits with a constraint result and the
  halt tag identifies `NOT NULL`, publish condition `1048 / 23000`;
- when the halt tag identifies `UNIQUE`, publish condition `1062 / 23000`;
- primary-key uniqueness uses SQLite's primary-key extended result code but the
  same unique halt tag, so it maps to the same MySQL duplicate-entry condition;
- leave `CHECK` and foreign-key halt tags untouched until their exact MySQL
  mapping is specified.

No grammar changes are required. This is a VDBE/runtime fork point.

## Runtime Semantics

For direct SQLite execution through the configured MyLite fork:

- a native `NOT NULL` constraint failure sets the fork condition level to
  error, MySQL errno `1048`, and SQLSTATE `23000`;
- native `UNIQUE` and `PRIMARY KEY` failures set error, MySQL errno `1062`,
  and SQLSTATE `23000`;
- successful statements do not publish these conditions;
- callers that inspect `sqlite3_errmsg()` still see SQLite's current message
  text in this slice;
- public MyLite callers that route a physical SQLite error through
  `mylite_diagnostics_set_sqlite_error()` receive an appended MySQL warning
  code matching the fork condition.

## Compatibility Status

MyLite now has fork-level MySQL condition metadata for native `NOT NULL`,
`UNIQUE`, and `PRIMARY KEY` constraint failures. Exact MySQL message rendering,
warning demotion, `CHECK`, foreign-key, generated-column, and broader
constraint/affected-row mapping remain deferred.
