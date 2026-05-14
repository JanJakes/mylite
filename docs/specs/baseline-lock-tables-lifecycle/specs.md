# Baseline LOCK TABLES Lifecycle

## Status

This feature specifies the first MyLite `LOCK TABLES` / `UNLOCK TABLES` slice.
It is intended for common dump, migration, and maintenance SQL that expects
MySQL to accept explicit table-lock statements around otherwise ordinary DDL
and DML. It builds on the current parser scaffold, durable schema/table
descriptors, temporary table shadowing rules, statement context, transaction
lifecycle, and descriptor-driven table resolution.

The feature is intentionally not full MySQL table-lock enforcement. MyLite
parses and resolves the admitted lock targets, records connection-local lock
intent, applies the verified transaction side effects, and releases the lock
intent through the verified statement paths. It does not block other handles or
reject later statements that access unlisted tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline transaction lifecycle:
  `docs/specs/baseline-transaction-lifecycle/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, `LOCK TABLES` / `UNLOCK TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/lock-tables.html
- MySQL 8.4 Reference Manual, implicit commit statements:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, transaction statements:
  https://dev.mysql.com/doc/refman/8.4/en/commit.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_lock_tables_lifecycle_expectations.sh`
records runtime probes for this feature. Observed behavior that shapes this
slice:

- `LOCK TABLES t READ`, `LOCK TABLE t WRITE`, `LOCK TABLES t READ LOCAL`, and
  comma-separated target lists succeed for existing tables.
- `LOCK TABLE` is a synonym for `LOCK TABLES`; `UNLOCK TABLE` is a synonym for
  `UNLOCK TABLES`.
- Successful `LOCK TABLES` and `UNLOCK TABLES` statements return no result
  rows, leave `ROW_COUNT()` at `0`, and leave `@@warning_count` at `0` in the
  probed in-range cases.
- `LOCK TABLES` without a selected schema for an unqualified table returns
  `1046 / 3D000`. Unknown schemas return `1049 / 42000`; unknown tables return
  `1146 / 42S02`.
- Repeating the same effective lock alias in one `LOCK TABLES` statement
  returns `1066 / 42000`.
- Effective lock alias comparison is case-sensitive in the observed runtime
  probes; `t` and `T` are distinct effective lock names.
- `LOCK TABLES` implicitly commits an active transaction before acquiring table
  locks. A later `ROLLBACK` does not undo work committed by the lock statement.
- `LOCK TABLES` releases existing table locks before acquiring new ones.
- If `LOCK TABLES` reaches runtime target acquisition and then fails because a
  target is missing, the active transaction has still been committed and the
  previous lock set has still been released.
- Beginning a transaction with `START TRANSACTION` releases existing table
  locks. The inserted row in the new transaction is still rolled back normally.
- `UNLOCK TABLES` releases current table locks. When locks from `LOCK TABLES`
  are active, MySQL treats it as a transaction-control/locking statement with
  implicit commit behavior.
- `LOCK TABLES` against a temporary table is accepted and ignored by MySQL for
  lock enforcement.
- MySQL enforces later locked-table access rules, alias requirements, read-lock
  write rejection, cross-session blocking, and DDL restrictions while locks are
  held. MyLite defers those enforcement rules in this first baseline slice.

## Scope

The implementation must add:

- parser and AST support for `LOCK TABLE[S]` with one or more table targets and
  `UNLOCK TABLE[S]`;
- `READ`, `READ LOCAL`, and `WRITE` lock modes;
- optional target aliases with `AS alias` or a bare alias;
- unqualified and schema-qualified target table resolution through the existing
  selected/default schema policy;
- descriptor resolution for persistent base tables and shadowing session
  temporary base tables;
- deterministic rejection of reserved `_mylite_*` schemas or table names before
  any SQLite SQL is generated;
- MySQL-compatible diagnostics for missing default schema, unknown schema,
  unknown table, and duplicate lock aliases in the admitted subset;
- connection-local recording of admitted lock intent: resolved schema, logical
  table name, optional alias, temporary/persistent object kind, and lock mode;
- release of existing recorded lock intent before acquiring a new lock set;
- release of recorded lock intent on `UNLOCK TABLE[S]`, `START TRANSACTION` /
  `BEGIN`, and connection close;
- implicit commit of an active MyLite user transaction before successful
  `LOCK TABLE[S]`;
- non-row successful result objects with affected rows `0` and warning count
  `0`;
- tests and MySQL 8.4.9 expectation artifacts for successful syntax,
  diagnostics, transaction side effects, persistence safety, and intentional
  deferrals.

## Non-Goals

This feature must not implement:

- cross-handle or cross-process blocking;
- rejecting writes through a recorded `READ` lock;
- rejecting access to tables that were not named in the current lock set;
- enforcing alias-only access after locking a table with an alias;
- DDL restrictions while locks are active;
- implicit trigger, view, foreign-key-related, Performance Schema, or `mysql`
  schema locks;
- privilege checks;
- `LOW_PRIORITY WRITE`, `WRITE LOW_PRIORITY`, or other lock modes;
- `LOCK INSTANCE FOR BACKUP`, `UNLOCK INSTANCE`, `FLUSH TABLES WITH READ LOCK`,
  global read locks, metadata-lock instrumentation, Performance Schema lock
  rows, or protocol status flags;
- storage-level SQLite locking changes or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, diagnostics plumbing, and cleanup.
- Statement context owns completion state, previous diagnostics snapshots,
  affected rows, and warning count. Successful lock statements are ordinary
  non-row statements with affected rows `0`.
- Lexer/parser/AST own syntax admission, target alias capture, lock-mode
  capture, and source spans. They do not inspect descriptors or SQLite.
- Analyzer/runtime owns schema resolution, duplicate alias checks, reserved
  name rejection, connection-local lock-intent storage, transaction side
  effects, and result construction.
- The catalog remains the authority for persistent schemas and base-table
  descriptors. Lock statements read descriptor metadata but do not mutate
  catalog rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Temporary table state remains connection-local and shadows persistent tables
  according to the existing resolution policy.
- SQLite physical storage is not asked to acquire table locks for this slice.
  MyLite uses existing transaction-control SQL only for verified implicit
  commit behavior.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. Lock
  statements must not touch the preamble or generate user-data writes except
  when committing an already active transaction.

## Supported SQL Grammar

Supported forms:

```sql
LOCK TABLE table_name [AS] alias lock_type
LOCK TABLES table_name [AS] alias lock_type [, table_name [AS] alias lock_type] ...
UNLOCK TABLE
UNLOCK TABLES
```

The alias is optional. `table_name` may be unqualified or schema-qualified.

```sql
lock_type:
    READ
  | READ LOCAL
  | WRITE
```

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement(A) ::= table_lock_statement(B). { A = B; }

table_lock_statement(A) ::= LOCK table_or_tables lock_table_list(L). {
    A = mylite_sql_parser_make_lock_tables_statement(state, &LOCK, L);
}
table_lock_statement(A) ::= UNLOCK table_or_tables(U). {
    A = mylite_sql_parser_make_unlock_tables_statement(state, &UNLOCK, &U);
}

table_or_tables ::= TABLE.
table_or_tables ::= TABLES.

lock_table_list(A) ::= lock_table_target(T). {
    A = mylite_sql_parser_make_lock_table_list(state, T);
}
lock_table_list(A) ::= lock_table_list(L) COMMA lock_table_target(T). {
    A = mylite_sql_parser_append_lock_table_target(state, L, T);
}

lock_table_target(A) ::= table_name(T) lock_table_alias_opt(B) lock_type(M). {
    A = mylite_sql_parser_make_lock_table_target(state, T, B, M);
}

lock_table_alias_opt(A) ::= . { A = NULL; }
lock_table_alias_opt(A) ::= AS identifier(I). { A = I; }
lock_table_alias_opt(A) ::= identifier(I). { A = I; }

lock_type(A) ::= READ(R). { A = MYLITE_SQL_AST_LOCK_TABLE_READ; }
lock_type(A) ::= READ(R) LOCAL(L). { A = MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL; }
lock_type(A) ::= WRITE(W). { A = MYLITE_SQL_AST_LOCK_TABLE_WRITE; }
```

Unsupported grammar, such as missing lock type, repeated `LOCAL`, extra lock
modifiers, instance backup locks, and empty target lists, remains a parse error.

## Name Resolution And Diagnostics

Each lock target resolves with the same schema policy as descriptor-backed table
statements:

- unqualified targets require a selected schema unless a matching temporary
  table shadows the name;
- schema-qualified targets use the named schema;
- temporary tables shadow persistent tables for unqualified resolution;
- unknown schemas return `1049 / 42000`;
- unknown tables return `1146 / 42S02`;
- missing default schema returns `1046 / 3D000`;
- reserved `_mylite_*` schema or table names are rejected with the existing
  MyLite reserved-name diagnostic;
- non-base object kinds must be rejected once descriptors for those objects
  exist.

Duplicate detection uses the effective lock name MySQL exposes for the lock
set: the target alias when present, otherwise the target table name. Duplicate
effective names with identical spelling in a single `LOCK TABLES` statement
return `1066 / 42000`. This baseline intentionally preserves the observed
case-sensitive comparison for effective lock names.

## Runtime Semantics

`LOCK TABLE[S]`:

1. If an active user transaction exists, commit it and clear user savepoints.
2. Release any previously recorded lock intent.
3. Resolve every target and validate duplicates.
4. Store the new connection-local lock-intent list.
5. Return a non-row successful result object with affected rows `0` and warning
   count `0`.

If target acquisition fails after statement execution begins, the transaction
commit and previous-lock release have already happened. Syntax errors and
public API misuse are still rejected before runtime lock side effects.

`UNLOCK TABLE[S]`:

1. If recorded lock intent exists and a user transaction is active, commit it
   and clear user savepoints. This mirrors the verified transaction-control
   behavior for locks acquired by `LOCK TABLES`.
2. Release recorded lock intent.
3. Return a non-row successful result object with affected rows `0` and warning
   count `0`.

`START TRANSACTION` / `BEGIN` release recorded table-lock intent before starting
the new user transaction. `COMMIT` and `ROLLBACK` do not release recorded table
lock intent in this slice, matching the verified MySQL distinction, although
the current slice does not enforce later access restrictions.

Connection close frees recorded lock intent after rolling back any active user
transaction. Reopened handles never inherit connection-local lock intent.

## Physical SQLite Handling

This feature is implemented as MyLite wrapper/translation logic. It does not
generate SQLite table-lock SQL and does not require a SQLite fork patch.
`LOCK TABLES` may run existing SQLite `COMMIT` through the transaction helper
when an active MyLite user transaction must be committed. Otherwise, supported
lock and unlock statements do not touch SQLite physical storage.

## Result Reporting

Successful lock statements return through the existing non-row public result
conventions:

- `mylite_result_column_count(result) == 0`;
- `mylite_result_row_count(result) == 0`;
- `mylite_result_affected_rows(result) == 0`;
- `mylite_result_warning_count(result) == 0`;
- a later `SELECT ROW_COUNT(), @@warning_count` observes `0, 0`.

Unsupported syntax or resolution failures return no successful result and set
the current statement diagnostic.

## Tests

Tests must cover:

- parser acceptance of singular/plural `LOCK` and `UNLOCK`, `READ`, `READ
  LOCAL`, `WRITE`, aliases, schema-qualified targets, and multiple targets;
- parser rejection of missing lock types, repeated `LOCAL`, unsupported
  modifiers, and backup/instance forms;
- successful persistent and temporary table locks;
- unqualified and schema-qualified resolution, including missing default
  schema, unknown schema, unknown table, duplicate effective aliases, and
  reserved names;
- case-sensitive effective lock alias handling;
- replacement of previous lock intent by a later `LOCK TABLES`;
- failed replacement releasing previous lock intent;
- `UNLOCK TABLE[S]` with and without an active lock set;
- implicit commit before `LOCK TABLES`;
- implicit commit before `LOCK TABLES` runtime failures;
- `START TRANSACTION` releasing lock intent and then rolling back later changes
  normally;
- `COMMIT` and `ROLLBACK` preserving lock intent internally without changing
  the admitted no-enforcement behavior;
- close/reopen clearing lock intent and preserving committed rows;
- independent handles with independent lock intent;
- `.mylite` preamble preservation for lock/unlock statements;
- zero-initialized cleanup of any new lock-intent storage;
- no public API changes.

Existing parser, runtime transaction, table lifecycle, temporary table, DML,
file-backed opening, VFS, statement-context, and result tests must continue to
pass.

## Compatibility Documentation

Update only the exact supported subset:

- `COMPATIBILITY.md` marks `LOCK TABLES` and `UNLOCK TABLES` as partial.
- `docs/compatibility/sql-locking.md` documents accepted syntax, resolution,
  transaction side effects, connection-local lock-intent recording, and the
  deliberately deferred enforcement/blocking semantics.
- `docs/compatibility/sql-transactions.md` may mention that `LOCK TABLES`
  participates in the current implicit-commit/release baseline if needed.

Do not claim full MySQL table-lock enforcement, metadata locks, privilege
checks, cross-session blocking, or DDL restrictions.
