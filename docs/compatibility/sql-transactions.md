# SQL transactions

Transaction control and savepoint compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `START TRANSACTION` | 🟡 | Limited explicit user transaction start for current supported transactional DML; starting while active commits the previous user transaction first; no `WITH CONSISTENT SNAPSHOT`, `READ WRITE`, `READ ONLY`, isolation/access modes, locks, protocol flags, or XA |
| `BEGIN` / `BEGIN WORK` | 🟡 | Limited aliases for the same start-transaction behavior as `START TRANSACTION`; no compound-statement grammar |
| `COMMIT` | 🟡 | Limited commit of the active user transaction, or no-op success when none is active; optional `WORK` is admitted, but `AND [NO] CHAIN` and `[NO] RELEASE` are not |
| `ROLLBACK` | 🟡 | Limited rollback of the active user transaction, or no-op success when none is active; optional `WORK` is admitted, but `AND [NO] CHAIN` and `[NO] RELEASE` are not |
| Autocommit mode | 🟡 | Limited scalar `@@autocommit` reads and fixed no-op `SET autocommit = 1` forms report/preserve visible value `1`; explicit transactions are started only with `START TRANSACTION` / `BEGIN`; no mutable `SET autocommit = 0`, protocol status flags, or full autocommit side effects |
| `SAVEPOINT` | 🟡 | Limited user-visible savepoint creation and same-name replacement inside current explicit MyLite user transactions; outside a transaction it is a MySQL-compatible no-op success under the fixed autocommit baseline; no stored-program savepoint levels, mutable autocommit, protocol flags, or isolation/access semantics |
| `ROLLBACK TO SAVEPOINT` | 🟡 | Limited partial rollback for current user-visible savepoints, including `ROLLBACK TO`, `ROLLBACK TO SAVEPOINT`, and `ROLLBACK WORK TO [SAVEPOINT]`; missing savepoints return MySQL-compatible error `1305`; no stored-program scoping or lock/protocol semantics |
| `RELEASE SAVEPOINT` | 🟡 | Limited release of current user-visible savepoints and later savepoints in the active explicit transaction, with MySQL-compatible missing-savepoint diagnostics; no stored-program scoping or protocol semantics |
| `SET TRANSACTION` | ❌ | Isolation and access scope |

Current supported DML participates in explicit user transactions with
statement-level rollback. User-visible savepoints can roll back or release work
inside those explicit transactions and are cleared by `COMMIT`, full
`ROLLBACK`, nested `START TRANSACTION`, supported DDL implicit commits, and
`mylite_close()`. Current supported DDL/object lifecycle statements implicitly
commit an active user transaction before they run, matching the baseline MySQL
behavior for permanent objects. `mylite_close()` rolls back an active
uncommitted transaction.

[Back to compatibility overview](../../COMPATIBILITY.md)
