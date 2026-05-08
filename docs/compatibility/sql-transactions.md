# SQL transactions

Transaction control and savepoint compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `START TRANSACTION` | ❌ | Start modifiers |
| `BEGIN` / `BEGIN WORK` | ❌ | Transaction begin semantics |
| `COMMIT` | ❌ | Completion modifiers |
| `ROLLBACK` | ❌ | Completion modifiers |
| Autocommit mode | 🟡 | Limited scalar `@@autocommit` reads report fixed enabled value `1`; no `SET autocommit`, explicit transaction lifecycle, implicit commit rules, rollback behavior, or status flags |
| `SAVEPOINT` | ❌ | Nested savepoint creation and replacement semantics |
| `ROLLBACK TO SAVEPOINT` | ❌ | Partial rollback semantics and errors |
| `RELEASE SAVEPOINT` | ❌ | Savepoint release semantics and errors |
| `SET TRANSACTION` | ❌ | Isolation and access scope |

[Back to compatibility overview](../../COMPATIBILITY.md)
