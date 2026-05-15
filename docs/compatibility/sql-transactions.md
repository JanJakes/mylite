# SQL transactions

Transaction control and savepoint compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `START TRANSACTION` | 🟡 | Limited explicit user transaction start for current supported transactional DML; starting while active commits the previous user transaction first; statement-level `READ ONLY` / `READ WRITE` and pending `SET TRANSACTION` characteristics are applied; `WITH CONSISTENT SNAPSHOT` is admitted with MySQL-compatible warning behavior for non-`REPEATABLE READ`; no MVCC snapshot semantics, mutable autocommit, locks, protocol flags, or XA |
| `BEGIN` / `BEGIN WORK` | 🟡 | Limited aliases for the same start-transaction behavior as `START TRANSACTION`; no compound-statement grammar |
| `BEGIN IMMEDIATE` | 🟡 | MyLite-specific compatibility extension for SQLite-oriented clients and test harnesses; maps to the existing `BEGIN` / `START TRANSACTION` lifecycle with no separate SQLite pass-through. Not MySQL syntax; no `DEFERRED`, `EXCLUSIVE`, `TRANSACTION`, or other SQLite transaction spellings |
| `COMMIT` | 🟡 | Limited commit of the active user transaction, or no-op success when none is active; optional `WORK` is admitted, but `AND [NO] CHAIN` and `[NO] RELEASE` are not |
| `ROLLBACK` | 🟡 | Limited rollback of the active user transaction, or no-op success when none is active; optional `WORK` is admitted, but `AND [NO] CHAIN` and `[NO] RELEASE` are not |
| Autocommit mode | 🟡 | Limited scalar `@@autocommit` reads and fixed no-op `SET autocommit = 1` forms report/preserve visible value `1`; explicit transactions are started only with `START TRANSACTION` / `BEGIN`; no mutable `SET autocommit = 0`, protocol status flags, or full autocommit side effects |
| `SAVEPOINT` | 🟡 | Limited user-visible savepoint creation and same-name replacement inside current explicit MyLite user transactions; outside a transaction it is a MySQL-compatible no-op success under the fixed autocommit baseline; no stored-program savepoint levels, mutable autocommit, or protocol flags |
| `ROLLBACK TO SAVEPOINT` | 🟡 | Limited partial rollback for current user-visible savepoints, including `ROLLBACK TO`, `ROLLBACK TO SAVEPOINT`, and `ROLLBACK WORK TO [SAVEPOINT]`; missing savepoints return MySQL-compatible error `1305`; no stored-program scoping or lock/protocol semantics |
| `RELEASE SAVEPOINT` | 🟡 | Limited release of current user-visible savepoints and later savepoints in the active explicit transaction, with MySQL-compatible missing-savepoint diagnostics; no stored-program scoping or protocol semantics |
| `SET TRANSACTION` | 🟡 | Limited `SET [SESSION] TRANSACTION` isolation/access characteristics for the current connection; `READ ONLY` rejects persistent-table DML while allowing temporary-table DML; pending values are consumed by `START TRANSACTION` and other transaction-consuming statements; isolation is tracked without adding new concurrency semantics; no `GLOBAL`, system-variable readback, privileges, lock semantics, or protocol flags |

Current supported DML participates in explicit user transactions with
statement-level rollback. User-visible savepoints can roll back or release work
inside those explicit transactions and are cleared by `COMMIT`, full
`ROLLBACK`, nested `START TRANSACTION`, supported `LOCK TABLES` transaction
effects, supported DDL implicit commits, and `mylite_close()`. Current
supported DDL/object lifecycle statements and limited `LOCK TABLES` implicitly
commit an active user transaction before they run; for `LOCK TABLES`, verified
runtime target-acquisition failures also keep that commit effect and release
previous lock intent. This matches the baseline MySQL behavior for permanent
objects and explicit table locks. `mylite_close()` rolls back an active
uncommitted transaction.

`SET TRANSACTION` state is connection-local. Next-transaction characteristics
are consumed by successful transaction-consuming statements, and read-only
write failures keep the pending characteristic available for the next
transaction attempt.

[Back to compatibility overview](../../COMPATIBILITY.md)
