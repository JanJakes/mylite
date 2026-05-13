# SQL transactions

Transaction control and savepoint compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `START TRANSACTION` | 🟡 | Limited explicit user transaction start for current supported transactional DML; starting while active commits the previous user transaction first; no `WITH CONSISTENT SNAPSHOT`, `READ WRITE`, `READ ONLY`, isolation/access modes, locks, protocol flags, or XA |
| `BEGIN` / `BEGIN WORK` | 🟡 | Limited aliases for the same start-transaction behavior as `START TRANSACTION`; no compound-statement grammar |
| `COMMIT` | 🟡 | Limited commit of the active user transaction, or no-op success when none is active; optional `WORK` is admitted, but `AND [NO] CHAIN` and `[NO] RELEASE` are not |
| `ROLLBACK` | 🟡 | Limited rollback of the active user transaction, or no-op success when none is active; optional `WORK` is admitted, but `AND [NO] CHAIN` and `[NO] RELEASE` are not |
| Autocommit mode | 🟡 | Limited scalar `@@autocommit` reads and fixed no-op `SET autocommit = 1` forms report/preserve visible value `1`; explicit transactions are started only with `START TRANSACTION` / `BEGIN`; no mutable `SET autocommit = 0`, protocol status flags, or full autocommit side effects |
| `SAVEPOINT` | ❌ | Nested savepoint creation and replacement semantics |
| `ROLLBACK TO SAVEPOINT` | ❌ | Partial rollback semantics and errors |
| `RELEASE SAVEPOINT` | ❌ | Savepoint release semantics and errors |
| `SET TRANSACTION` | ❌ | Isolation and access scope |

Current supported DML participates in explicit user transactions with
statement-level rollback. Current supported DDL/object lifecycle statements
implicitly commit an active user transaction before they run, matching the
baseline MySQL behavior for permanent objects. `mylite_close()` rolls back an
active uncommitted transaction.

[Back to compatibility overview](../../COMPATIBILITY.md)
