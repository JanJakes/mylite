# SQL transactions

Transaction control and savepoint compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `START TRANSACTION` | ❌ | Start modifiers |
| `BEGIN` / `BEGIN WORK` | ❌ | Transaction begin semantics |
| `COMMIT` | ❌ | Completion modifiers |
| `ROLLBACK` | ❌ | Completion modifiers |
| `SAVEPOINT` | ❌ | Nested savepoint creation and replacement semantics |
| `ROLLBACK TO SAVEPOINT` | ❌ | Partial rollback semantics and errors |
| `RELEASE SAVEPOINT` | ❌ | Savepoint release semantics and errors |
| `SET TRANSACTION` | ❌ | Isolation and access scope |

[Back to compatibility overview](../../COMPATIBILITY.md)
