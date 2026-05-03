# SQLite BEGIN CONCURRENT Prototype

## Scope

This prototype evaluates SQLite's experimental `begin-concurrent` branch as a
storage primitive for overlapping MyLite write transactions.

This is not a MySQL compatibility feature. MySQL 8.4 does not define
`BEGIN CONCURRENT`, and portable MySQL applications must not depend on it.
MyLite accepts the syntax only as an experimental extension while this branch
measures whether the underlying SQLite behavior is useful.

In scope:

- vendoring a generated SQLite amalgamation from SQLite's `begin-concurrent`
  branch
- parsing `BEGIN CONCURRENT` as a MyLite transaction starter
- forwarding that starter to SQLite as `BEGIN CONCURRENT`
- enabling WAL mode for file-backed `.mylite` opens
- preserving the concurrent transaction mode across `COMMIT AND CHAIN` and
  `ROLLBACK AND CHAIN`
- focused tests that verify two file-backed MyLite connections can both write
  before either transaction finishes

Out of scope:

- presenting `BEGIN CONCURRENT` as MySQL-compatible SQL
- supporting `START TRANSACTION CONCURRENT` or `BEGIN CONCURRENT WORK`
- changing normal `BEGIN`, `BEGIN WORK`, or `START TRANSACTION` behavior
- conflict retry policy after `SQLITE_BUSY_SNAPSHOT`
- WAL2, server-style lock wait timeouts, deadlock diagnostics, or MVCC
  semantics beyond what the experimental SQLite branch provides
- making WAL sidecars part of MyLite's eventual single-file guarantee

## Sources

- SQLite source branch `begin-concurrent`, manifest
  `7f954a9e2fa4203b55825dfd70a46ffde7c985a4c8b940208d74d97441f3fd04`.
- SQLite source snapshot:
  `https://sqlite.org/src/tarball/sqlite-begin-concurrent.tar.gz?r=begin-concurrent`
- Vendoring metadata in `third_party/sqlite/source.json`.
- SQLite WAL documentation: https://www.sqlite.org/wal.html
- Existing MyLite specs:
  - `docs/specs/mylite-file-format/specs.md`
  - `docs/specs/transaction-statements/specs.md`

This specification is independently authored from public SQLite behavior,
MyLite source, and local runtime tests. It does not copy SQLite implementation
code, generated grammar, or documentation prose.

## Behavior

`BEGIN CONCURRENT` starts an explicit read/write transaction. It returns the
same MyLite C API shape as `BEGIN`: zero columns, `MYLITE_DONE`, and affected
rows `0`.

When another explicit MyLite transaction is already active on the same handle,
`BEGIN CONCURRENT` follows the existing MyLite transaction-starter rule: commit
the active transaction first, then start the new one.

`COMMIT`, `COMMIT WORK`, `ROLLBACK`, and `ROLLBACK WORK` end a concurrent
transaction through the same MyLite paths as ordinary explicit transactions.
When `AND CHAIN` is requested, the replacement transaction uses the same
concurrent mode as the transaction that just ended.

`BEGIN CONCURRENT` does not imply MySQL `WITH CONSISTENT SNAPSHOT`, isolation
level changes, or read-only state. MyLite's existing read-only transaction
checks continue to apply only to `START TRANSACTION READ ONLY`.

## SQLite Storage Policy

File-backed `mylite_open()` enables `PRAGMA journal_mode=WAL` immediately after
opening the SQLite handle and before seeding MyLite catalog tables. If SQLite
does not report `wal`, opening the database fails with `MYLITE_SQLITE_ERROR`.

`mylite_open_memory()` does not request WAL mode. In-memory databases may parse
`BEGIN CONCURRENT`, but they do not provide useful multi-connection write
concurrency for this prototype.

MyLite's VFS continues to shift only the main database payload by the `.mylite`
preamble offset. WAL and shared-memory sidecars are delegated to SQLite's
underlying VFS without a MyLite preamble, as described by the file-format spec.

## Parser And AST

`CONCURRENT` is a non-reserved keyword in MyLite's lexer/parser. It can still
be used as an identifier outside the transaction starter production.

Independently authored Lemon-style grammar sketch:

```lemon
begin_transaction_statement(A) ::= BEGIN(T) opt_work(W). {
    A = make_begin_transaction_statement(state, T, W, false);
}
begin_transaction_statement(A) ::= BEGIN(T) CONCURRENT(C). {
    A = make_begin_transaction_statement(state, T, C, true);
}
```

The `MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT` node carries a
`transaction_concurrent` flag. Runtime statement planning copies that flag into
the transaction plan.

Rejected syntax:

- `BEGIN CONCURRENT WORK`
- `START TRANSACTION CONCURRENT`
- `BEGIN READ ONLY`
- `BEGIN WORK READ ONLY`

## Runtime Notes

Statement atomicity inside explicit transactions remains savepoint-based.
MyLite does not start nested SQLite transactions after `BEGIN CONCURRENT`.

The current prototype does not implement retry or application-level conflict
resolution. If SQLite reports a commit conflict, MyLite surfaces the SQLite
error through its existing `MYLITE_SQLITE_ERROR` path.

DDL inside a concurrent transaction is not a supported compatibility contract.
SQLite's experimental branch has restrictions around schema changes during
concurrent transactions, and MyLite does not add a DDL-specific abstraction in
this prototype.

## Test Plan

Parser tests cover:

- `BEGIN CONCURRENT` parses as a begin-transaction statement
- the AST carries the concurrent flag and preserves the source span
- `CONCURRENT` remains usable as an identifier
- unsupported mixed forms are syntax errors

Runtime tests cover:

- file-backed `mylite_open()` leaves SQLite in WAL mode when accessed through
  the MyLite VFS
- two MyLite handles can each run `BEGIN CONCURRENT`
- both handles can insert into the same logical table before either handle
  commits or rolls back
- committing one handle and rolling back the other leaves only the committed
  row visible
- `COMMIT AND CHAIN` preserves concurrent mode for the replacement transaction
