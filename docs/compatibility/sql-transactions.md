# SQL transactions

Transaction control and savepoint compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `START TRANSACTION` | 🟡 | Limited explicit user transaction start for current supported transactional DML; starting while active commits the previous user transaction first, including pending autocommit-disabled work; statement-level `READ ONLY` / `READ WRITE` and pending `SET TRANSACTION` characteristics are applied; `WITH CONSISTENT SNAPSHOT` is admitted with MySQL-compatible warning behavior for non-`REPEATABLE READ`; no MVCC snapshot semantics, locks, protocol flags, or XA |
| `BEGIN` / `BEGIN WORK` | 🟡 | Limited aliases for the same start-transaction behavior as `START TRANSACTION`; no compound-statement grammar |
| `BEGIN IMMEDIATE` | 🟡 | MyLite-specific compatibility extension for SQLite-oriented clients and test harnesses; maps to the existing `BEGIN` / `START TRANSACTION` lifecycle with no separate SQLite pass-through. Not MySQL syntax; no `DEFERRED`, `EXCLUSIVE`, `TRANSACTION`, or other SQLite transaction spellings |
| `COMMIT` | 🟡 | Limited commit of the active user transaction, or no-op success when none is active; admits optional `WORK`, `AND [NO] CHAIN`, and `[NO] RELEASE`; `AND CHAIN` starts a new explicit MyLite user transaction preserving the just-ended transaction isolation/access mode when available, while `RELEASE` is accepted without closing the embedded handle |
| `ROLLBACK` | 🟡 | Limited rollback of the active user transaction, or no-op success when none is active; admits optional `WORK`, `AND [NO] CHAIN`, and `[NO] RELEASE`; `AND CHAIN` starts a new explicit MyLite user transaction preserving the just-ended transaction isolation/access mode when available, while `RELEASE` is accepted without closing the embedded handle |
| Autocommit mode | 🟡 | Session-local `SET autocommit` and readback are supported for current DML transaction side effects: disabled mode keeps supported writes pending until `COMMIT`, `ROLLBACK`, `SET autocommit=1`, implicit commit, or close-time rollback; global mutation, protocol flags, MVCC snapshot parity, locks, and full server transaction semantics remain unsupported |
| `SAVEPOINT` | 🟡 | Limited user-visible savepoint creation and same-name replacement inside current explicit MyLite user transactions and autocommit-disabled transactions; outside a transaction while autocommit is enabled it is a MySQL-compatible no-op success; no stored-program savepoint levels or protocol flags |
| `ROLLBACK TO SAVEPOINT` | 🟡 | Limited partial rollback for current user-visible savepoints, including `ROLLBACK TO`, `ROLLBACK TO SAVEPOINT`, and `ROLLBACK WORK TO [SAVEPOINT]`; temporary create/drop descriptors are reconciled to MySQL non-rollbackable DDL semantics while row changes roll back; missing savepoints return MySQL-compatible error `1305`; no stored-program scoping or lock/protocol semantics |
| `RELEASE SAVEPOINT` | 🟡 | Limited release of current user-visible savepoints and later savepoints in the active explicit transaction, with MySQL-compatible missing-savepoint diagnostics; no stored-program scoping or protocol semantics |
| `SET TRANSACTION` | 🟡 | Limited `SET [SESSION] TRANSACTION` isolation/access characteristics for the current connection; `READ ONLY` rejects persistent-table DML while allowing temporary-table DML; pending values are consumed by `START TRANSACTION` and other transaction-consuming statements; related transaction system variables expose and update the same session and next-transaction state; isolation is tracked without adding new concurrency semantics; no mutable `GLOBAL`, privileges, lock semantics, or protocol flags |

Current supported DML participates in explicit user transactions and
autocommit-disabled user transactions with statement-level rollback.
User-visible savepoints can roll back or release work inside those transactions
and are cleared by `COMMIT`, full `ROLLBACK`, nested `START TRANSACTION`,
supported `LOCK TABLES` transaction effects, supported DDL implicit commits,
and `mylite_close()`. Current supported DDL/object lifecycle statements and
limited `LOCK TABLES` implicitly commit an active user transaction before they
run; for `LOCK TABLES`, verified runtime target-acquisition failures also keep
that commit effect and release previous lock intent. This matches the baseline
MySQL behavior for permanent objects and explicit table locks. `mylite_close()`
rolls back an active uncommitted transaction.

`SET TRANSACTION` state is connection-local. Next-transaction characteristics
are consumed by successful transaction-consuming statements, and read-only
write failures keep the pending characteristic available for the next
transaction attempt.

Transaction completion modifiers follow MySQL's observable baseline where
`AND CHAIN` starts a new transaction after `COMMIT` or `ROLLBACK`, including
when no explicit user transaction was active. MyLite accepts `RELEASE` and
`NO RELEASE` syntax but does not close the embedded database handle because
there is no server-owned client session to disconnect.

`@@transaction_isolation` and `@@transaction_read_only` expose MyLite's
connection-local session transaction defaults. Direct session/local assignments
update those defaults, while direct `SET @@transaction_isolation = ...` and
`SET @@transaction_read_only = ...` update pending next-transaction
characteristics without changing scalar readback. Global transaction defaults
remain fixed embedded defaults with only exact no-op assignment forms admitted.

[Back to compatibility overview](../../COMPATIBILITY.md)
