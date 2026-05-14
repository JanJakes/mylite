# SQL locking

Instance and table locking statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `LOCK INSTANCE FOR BACKUP` | ❌ | Backup lock syntax and embedded behavior |
| `UNLOCK INSTANCE` | ❌ | Backup lock release syntax |
| `LOCK TABLES` | 🟡 | Limited `LOCK TABLE[S] table_name [[AS] alias] READ [LOCAL]\|WRITE[, ...]` over persistent or shadowing temporary base tables. The statement resolves targets through MyLite descriptors, rejects missing default schemas, unknown schemas/tables, reserved names, and exact-case duplicate effective lock aliases, records connection-local lock intent, releases any previous lock intent, and implicitly commits an active user transaction before target acquisition, including verified runtime target-acquisition failures. Temporary table locks are accepted as MySQL-compatible ignored lock intent. No cross-session blocking, read/write access enforcement, alias-only access requirements, DDL restrictions while locked, implicit trigger/view/foreign-key related locks, Performance Schema metadata-lock rows, or privilege checks |
| `UNLOCK TABLES` | 🟡 | Limited `UNLOCK TABLE[S]` releases current connection-local lock intent and returns a non-row success result with affected rows `0` and warning count `0`; no global read-lock release, metadata-lock instrumentation, or broader lock enforcement |

[Back to compatibility overview](../../COMPATIBILITY.md)
