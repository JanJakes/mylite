# Baseline BEGIN IMMEDIATE Compatibility

## Summary

This phase admits the narrow SQLite-style statement:

```sql
BEGIN IMMEDIATE
```

as a MyLite compatibility extension for client and test harnesses that already
open SQLite write transactions directly. It is not MySQL syntax. MyLite maps it
to the same user-visible transaction lifecycle as the existing `BEGIN` /
`BEGIN WORK` / `START TRANSACTION` subset, including pending transaction
characteristics, nested-start behavior, DML rollback/commit behavior, savepoint
clearing, and close-time rollback.

No additional SQLite SQL is exposed to users. Runtime execution stays on
MyLite's existing transaction-control path, which already opens the underlying
SQLite transaction with internal `BEGIN IMMEDIATE`.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing transaction design:
  - `docs/specs/baseline-transaction-lifecycle/specs.md`
  - `docs/specs/baseline-start-transaction-characteristics/specs.md`
  - `docs/specs/baseline-set-transaction/specs.md`
  - `docs/specs/baseline-savepoint-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual, transaction control:
  <https://dev.mysql.com/doc/refman/8.4/en/commit.html>
- SQLite transaction documentation:
  <https://www.sqlite.org/lang_transaction.html>

The MySQL 8.4 documentation lists `BEGIN [WORK]`, `START TRANSACTION`, `COMMIT`,
and `ROLLBACK` forms for transaction control. SQLite documents `BEGIN
IMMEDIATE` as a write-transaction starter. This phase is therefore explicitly a
MyLite extension, not a MySQL-supported feature claim. The existing MySQL 8.4.9
transaction expectation artifact remains the authority for the mapped
transaction behavior; no new MySQL runtime expectation script is required for
this extension-specific spelling.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 transaction behavior
already captured in the repository, SQLite public documentation, public SQLite
APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Scope

Admitted SQL:

```sql
BEGIN IMMEDIATE
```

The token `IMMEDIATE` is case-insensitive and must be unquoted. Comments and
ordinary whitespace may appear between tokens according to the existing lexer
rules.

Deferred forms:

```sql
BEGIN DEFERRED
BEGIN EXCLUSIVE
BEGIN TRANSACTION
BEGIN IMMEDIATE TRANSACTION
END TRANSACTION
COMMIT TRANSACTION
ROLLBACK TRANSACTION
```

The deferred forms remain syntax errors. MyLite keeps the extension narrow to
the reported compatibility path and does not admit wider SQLite transaction
grammar.

## Ownership Boundaries

- Public API: unchanged. Successful execution returns through the existing
  non-row `mylite_result` conventions with affected rows `0` and warning count
  `0`.
- Statement context: unchanged. Diagnostics, warning counts, result
  finalization, and wrapper transaction reporting continue to use existing
  transaction-control behavior.
- Lexer/parser/AST: admits `BEGIN IMMEDIATE` and represents it as the existing
  `MYLITE_SQL_AST_START_TRANSACTION_STATEMENT` with no characteristic child.
  `IMMEDIATE` remains an ordinary identifier outside the `BEGIN IMMEDIATE`
  grammar shape.
- Runtime: reuses the existing start-transaction handler. The spelling has no
  separate runtime branch after parsing.
- Catalog: read-only. The statement does not create, update, or invalidate
  catalog descriptors, catalog generation, descriptor caches, or SQLite schema
  generation.
- SQLite execution: uses MyLite's existing transaction-control helpers and
  internal `BEGIN IMMEDIATE` control statement. MyLite does not pass arbitrary
  SQLite transaction grammar through to the underlying engine.
- Storage/VFS/file format: unchanged. The `.mylite` preamble and shifted
  SQLite payload invariants are preserved.

## MyLite Lemon-Syntax Snippet

The parser grammar extension is independently authored and intentionally narrow:

```lemon
transaction_control_statement(A) ::= BEGIN(B) IDENTIFIER(I). {
    A = mylite_sql_parser_make_begin_immediate_statement(state, B, I);
}
```

The constructor accepts only case-insensitive token text `IMMEDIATE`; any other
identifier after `BEGIN` is a syntax error. The accepted statement spans the
full `BEGIN IMMEDIATE` text and produces a start-transaction AST node with no
characteristics.

## Runtime Semantics

`BEGIN IMMEDIATE` behaves exactly like the current supported `BEGIN` spelling:

- if no user transaction is active, MyLite starts a user transaction;
- if a user transaction is already active, MyLite commits it first, clears its
  savepoints and active characteristics, and starts a new user transaction;
- pending `SET TRANSACTION` characteristics are consumed by the new
  transaction in the same way as `BEGIN` / `START TRANSACTION`;
- supported DML inside the transaction can be committed or rolled back;
- supported DDL keeps the existing MySQL-style implicit-commit behavior;
- close-time rollback still rolls back an active uncommitted transaction;
- independent handles keep independent transaction state;
- successful execution returns no rows, affected rows `0`, and warning count
  `0`.

`BEGIN IMMEDIATE` does not expose SQLite's nested-`BEGIN` error behavior.
Nested starts keep MyLite's existing MySQL-compatible rule: beginning a new
transaction commits the prior active user transaction.

## Diagnostics

- Unsupported SQLite transaction spellings remain parser syntax errors using
  MyLite's existing parse diagnostic surface.
- Allocation failures follow existing parser/runtime allocation diagnostics.
- SQLite control failures from the underlying transaction start keep the
  existing normalized transaction-control diagnostic behavior.

## Tests

Add focused coverage to existing plain C tests:

- parser accepts `BEGIN IMMEDIATE` and mixed-case variants as
  `MYLITE_SQL_AST_START_TRANSACTION_STATEMENT`;
- parser rejects `BEGIN DEFERRED`, `BEGIN EXCLUSIVE`, and `BEGIN IMMEDIATE
  TRANSACTION`;
- `IMMEDIATE` remains usable as an ordinary identifier outside the extension
  grammar;
- runtime verifies successful non-row result shape, affected rows `0`, and
  warning count `0`;
- runtime verifies commit, rollback, nested-start commit, pending
  `SET TRANSACTION` consumption, close-time rollback, and file-backed
  persistence behavior through the existing transaction lifecycle test binary.

Compatibility docs must state clearly that this is a MyLite extension and not
MySQL syntax.
