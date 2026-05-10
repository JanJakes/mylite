# SQL transactions

Transaction control and savepoint compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `START TRANSACTION` | ❌ | Start modifiers |
| `BEGIN` / `BEGIN WORK` | ❌ | Transaction begin semantics |
| `COMMIT` | ❌ | Completion modifiers |
| `ROLLBACK` | ❌ | Completion modifiers |
| Autocommit mode | 🟡 | Limited scalar `@@autocommit` reads and fixed no-op `SET autocommit = 1` forms report/preserve fixed enabled value `1`; no mutable `SET autocommit = 0`, explicit transaction lifecycle, implicit commit rules, rollback behavior, or status flags |
| `SAVEPOINT` | ❌ | Nested savepoint creation and replacement semantics |
| `ROLLBACK TO SAVEPOINT` | ❌ | Partial rollback semantics and errors |
| `RELEASE SAVEPOINT` | ❌ | Savepoint release semantics and errors |
| `SET TRANSACTION` | ❌ | Isolation and access scope |

[Back to compatibility overview](../../COMPATIBILITY.md)
