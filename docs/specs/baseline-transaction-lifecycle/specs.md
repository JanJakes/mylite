# Baseline Transaction Lifecycle

## Status

This feature adds the first explicit transaction-control slice on top of
MyLite's existing statement-atomic DDL and DML execution. The goal is the
smallest MySQL-compatible user transaction lifecycle that lets applications
group supported DML and roll it back or commit it explicitly.

This is not full MySQL transaction support. It does not add mutable
`autocommit`, savepoint statements, transaction isolation, read-only
transactions, row locking behavior, protocol status flags, XA, or complete
implicit-commit parity for every future statement. It does make current
supported DML participate in a user transaction and makes current supported DDL
perform MySQL-compatible implicit commits around the active user transaction.

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
- MyLite file-format preamble: `docs/specs/mylite-file-format/specs.md`
- Existing DDL/DML lifecycle specs under `docs/specs/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, transaction control:
  https://dev.mysql.com/doc/refman/8.4/en/commit.html
- MySQL 8.4 Reference Manual, autocommit, commit, and rollback:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-autocommit-commit-rollback.html
- MySQL 8.4 Reference Manual, statements that cannot be rolled back:
  https://dev.mysql.com/doc/refman/8.4/en/cannot-roll-back.html
- MySQL 8.4 Reference Manual, implicit commit:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, InnoDB error handling:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-error-handling.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_transaction_lifecycle_expectations.sh`
records the MySQL 8.4.9 expectations for this slice:

- `COMMIT` and `ROLLBACK` outside an active transaction succeed, return no
  rows, make `ROW_COUNT()` return `0`, and produce no warnings.
- `START TRANSACTION`, `BEGIN`, and `BEGIN WORK` start a user transaction while
  leaving `@@autocommit = 1`.
- `COMMIT` persists supported transactional DML from the active transaction.
- `ROLLBACK` discards supported transactional DML from the active transaction.
- `COMMIT WORK` and `ROLLBACK WORK` are synonyms for the supported completion
  forms.
- `START TRANSACTION` while a user transaction is active commits the old
  transaction first and starts a new one. Transactions are not nested.
- Supported DDL statements implicitly commit any active user transaction before
  they run. After the DDL completes, later DML runs in autocommit mode unless a
  new transaction is started.
- A duplicate-key statement error inside a transaction rolls back only the
  failed statement. The transaction remains active and can commit later
  successful DML.
- Disconnecting an active session rolls back the uncommitted transaction.

## Scope

The implementation must add:

- parser and AST nodes for:
  - `START TRANSACTION`
  - `BEGIN`
  - `BEGIN WORK`
  - `COMMIT`
  - `COMMIT WORK`
  - `ROLLBACK`
  - `ROLLBACK WORK`
- connection-local user transaction state;
- transaction-control runtime handlers using existing non-row statement result
  conventions;
- internal statement transaction helpers that:
  - open a SQLite `BEGIN IMMEDIATE` transaction when no user transaction is
    active;
  - open a deterministic internal SQLite savepoint when a user transaction is
    active; and
  - commit/release or roll back only the current statement boundary;
- integration of current supported DML write statements with those helpers:
  `INSERT ... VALUES`, `INSERT ... SET`, descriptor-backed `INSERT ... SELECT`,
  no-key `REPLACE` forms, single-table `UPDATE`, and single-table `DELETE`;
- implicit user-transaction commit before current supported DDL/object
  lifecycle statements:
  `CREATE DATABASE`, `DROP DATABASE`, `CREATE TABLE`, `CREATE TABLE ... LIKE`,
  `CREATE TABLE ... SELECT`, `CREATE INDEX`, `DROP INDEX`, `DROP TABLE`,
  `TRUNCATE TABLE`, `RENAME TABLE`, and supported `ALTER TABLE` actions;
- rollback of an active user transaction during `mylite_close()`;
- file-backed persistence tests for committed data and rollback tests for
  uncommitted data;
- compatibility documentation for the exact supported subset.

## Non-Goals

This feature must not implement:

- `START TRANSACTION` modifiers: `WITH CONSISTENT SNAPSHOT`, `READ WRITE`,
  `READ ONLY`, or multiple characteristics;
- `COMMIT` / `ROLLBACK` completion modifiers: `AND CHAIN`, `AND NO CHAIN`,
  `RELEASE`, or `NO RELEASE`;
- autocommit protocol status flags; session-local `SET autocommit` transaction
  side effects are covered by `baseline-autocommit-system-variable`;
- user-visible `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`, or `RELEASE SAVEPOINT`
  syntax;
- `SET TRANSACTION`, isolation levels, access modes, consistent snapshots, lock
  release semantics, row locks, gap locks, deadlock or lock-wait diagnostics;
- XA transactions, binary logging, replication behavior, privilege checks,
  Performance Schema, `INFORMATION_SCHEMA.INNODB_TRX`, or process-list state
  changes beyond existing fixed `SHOW PROCESSLIST` rows;
- nontransactional-table warnings or mixed-storage-engine behavior;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own public
  misuse handling, result-handle lifetime, diagnostics, and previous result
  state.
- Session state owns the connection-local user transaction flag. This is not a
  public ABI field.
- Statement context owns each top-level statement boundary and may record
  wrapper transaction state for tests and future protocol integration. It does
  not own long-lived user transaction state.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Runtime dispatch owns transaction-control statements and the
  implicit-commit-before-DDL policy.
- Analyzer/planner code remains statement-specific. Existing DML planners keep
  resolving descriptors, conversions, generated SQL, and diagnostics exactly as
  before; only their physical transaction wrapper changes.
- Catalog remains the descriptor authority. DDL still mutates catalog rows
  through catalog mutation transactions, but only after any active user
  transaction has been committed according to MySQL's implicit-commit rule.
- SQLite owns physical b-tree storage, transaction durability, and internal
  savepoint mechanics. MyLite uses public SQLite SQL control statements and
  does not add a fork hook.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. User
  transaction control must not write the preamble.

## Supported SQL Grammar

Supported transaction-control statements:

```sql
START TRANSACTION
BEGIN
BEGIN WORK
COMMIT
COMMIT WORK
ROLLBACK
ROLLBACK WORK
```

Unsupported grammar:

```sql
START TRANSACTION WITH CONSISTENT SNAPSHOT
START TRANSACTION READ WRITE
START TRANSACTION READ ONLY
COMMIT AND CHAIN
COMMIT RELEASE
ROLLBACK AND NO CHAIN
ROLLBACK NO RELEASE
SAVEPOINT name
ROLLBACK TO SAVEPOINT name
RELEASE SAVEPOINT name
SET TRANSACTION ...
```

Unsupported forms may be syntax errors until a later phase specifies stable
MyLite diagnostics. They must not be silently accepted.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
statement ::= transaction_control_statement.

transaction_control_statement ::= START TRANSACTION.
transaction_control_statement ::= BEGIN.
transaction_control_statement ::= BEGIN WORK.
transaction_control_statement ::= COMMIT.
transaction_control_statement ::= COMMIT WORK.
transaction_control_statement ::= ROLLBACK.
transaction_control_statement ::= ROLLBACK WORK.
```

## Semantics

### User Transaction State

Each `mylite_db` handle tracks whether it has an active user transaction.
Independent handles have independent transaction state. In-memory and
file-backed handles use the same behavior.

`@@autocommit` remains the fixed visible value `1` in this phase. Starting a
user transaction does not change scalar `@@autocommit` readback.

### `START TRANSACTION` / `BEGIN`

If no user transaction is active, MyLite executes SQLite `BEGIN IMMEDIATE` and
marks the user transaction active. The result is an empty non-row result with
affected rows `0` and no warnings.

If a user transaction is already active, MyLite first commits the active SQLite
transaction, clears the active flag, then starts a new SQLite `BEGIN IMMEDIATE`
transaction and marks it active. This mirrors MySQL's non-nested transaction
model where starting a transaction implicitly commits the previous one.

`BEGIN WORK` is accepted as an alias of `BEGIN`.

### `COMMIT`

If a user transaction is active, MyLite executes SQLite `COMMIT` and clears the
active flag. If no user transaction is active, `COMMIT` succeeds as a no-op.

`COMMIT WORK` is accepted as a synonym. Completion modifiers are not admitted.

### `ROLLBACK`

If a user transaction is active, MyLite executes SQLite `ROLLBACK` and clears
the active flag. If no user transaction is active, `ROLLBACK` succeeds as a
no-op.

`ROLLBACK WORK` is accepted as a synonym. Completion modifiers are not
admitted.

### Statement Atomicity Inside User Transactions

Current supported DML write statements must remain statement-atomic inside a
user transaction. When a user transaction is active, each DML write statement
uses an internal SQLite savepoint:

```sql
SAVEPOINT _mylite_statement
-- generated descriptor-driven statement work
RELEASE SAVEPOINT _mylite_statement
```

On failure after the savepoint starts, MyLite rolls back to the savepoint and
then releases it. The user transaction remains active. This is required for
duplicate-key errors, conversion errors detected during execution, physical
SQLite failures, and allocation failures after the savepoint begins.

The internal savepoint name is never exposed as SQL syntax. User-visible
savepoint statements remain unsupported.

### Standalone Statement Transactions

When no user transaction is active, existing write statements use a MyLite-owned
SQLite `BEGIN IMMEDIATE` transaction and `COMMIT`/`ROLLBACK` as they do today.
This preserves current autocommit statement atomicity and file-backed
durability.

### DDL Implicit Commit

Before executing any current supported DDL/object lifecycle statement, MyLite
commits an active user transaction and clears the user transaction flag. The DDL
then runs in its existing statement transaction. A later `ROLLBACK` does not
undo the preceding DML or the DDL.

This feature intentionally implements only the "commit before DDL" side needed
for the current statement set. Future DDL forms must be added to the same
classification when they are implemented.

### Close Behavior

`mylite_close()` rolls back an active user transaction before closing SQLite.
The close function remains `void` and ignores rollback failures during cleanup,
matching the existing no-fail close API shape.

## Result And Diagnostics

Successful supported transaction-control statements:

- return `MYLITE_OK`;
- return a non-row result object;
- set `affected_rows == 0`;
- set `warning_count == 0`;
- make the next `ROW_COUNT()` return `0`;
- do not mutate schemas, descriptors, rows, or file bytes except through the
  requested transaction boundary.

Diagnostics:

- syntax errors use existing parser diagnostics;
- unsupported transaction modifiers use syntax errors or deterministic
  unsupported diagnostics once parsed;
- SQLite begin/commit/rollback failures use the existing physical SQLite
  diagnostic surface;
- allocation failures return `MYLITE_NOMEM`;
- public API misuse is unchanged.

## SQLite Handling

MyLite uses public SQLite SQL control statements:

- `BEGIN IMMEDIATE`
- `COMMIT`
- `ROLLBACK`
- `SAVEPOINT _mylite_statement`
- `ROLLBACK TO SAVEPOINT _mylite_statement`
- `RELEASE SAVEPOINT _mylite_statement`

No generated user-table identifiers or values are interpolated in these control
statements. Descriptor-driven DML keeps its existing quoted identifier and bound
parameter policy.

The savepoint name is static because MyLite executes only one statement at a
time per handle. Future concurrent statement execution would need a per-
statement generated savepoint name.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, preferably
`runtime_transaction_lifecycle_test.c`, registered as
`libmylite.runtime.transaction_lifecycle`.

Coverage must include:

- parser coverage for supported forms and rejected modifiers;
- `COMMIT` and `ROLLBACK` outside an active transaction;
- `START TRANSACTION`, `BEGIN`, and `BEGIN WORK` aliases;
- `COMMIT WORK` and `ROLLBACK WORK` aliases;
- `@@autocommit` remains `1` inside an active transaction;
- committed DML persists in memory and file-backed databases;
- rolled-back DML disappears in memory and file-backed databases;
- an active transaction rolls back on `mylite_close()`;
- `START TRANSACTION` inside an active transaction commits prior work and starts
  a new transaction;
- duplicate-key failure inside a transaction rolls back only the failed
  statement and leaves the transaction usable;
- current supported DML wrappers inside a user transaction: insert values,
  insert set, insert select, replace values/set/select no-key forms, update,
  and delete;
- DDL implicit commit before create/drop/rename/truncate/alter/index/schema
  statements where feasible without making the test too broad;
- independent file-backed handles with independent transaction state;
- `.mylite` preamble preservation after commit and rollback;
- existing lifecycle tests still pass.

Run:

1. `packages/libmylite/tests/mysql_baseline_transaction_lifecycle_expectations.sh`
2. `cmake --build --preset dev`
3. `ctest --test-dir build/dev -R 'libmylite\\.(parser|runtime\\.transaction_lifecycle|runtime\\.(row_values_lifecycle|update_lifecycle|delete_lifecycle|truncate_table_lifecycle|insert_select_lifecycle|replace_values_lifecycle|replace_select_lifecycle|table_rename_lifecycle|multi_table_rename_lifecycle|create_index_lifecycle|drop_index_lifecycle))$' --output-on-failure`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-transactions.md`
- `docs/compatibility/runtime-session-sql-modes.md`
- `docs/compatibility/runtime-system-variables.md`

The docs must say that explicit `START`/`COMMIT`/`ROLLBACK` transactions are
limited but supported, while unsupported savepoint details, isolation/access
modes, lock semantics, and protocol transaction flags remain unsupported.
