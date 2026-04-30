# SQL tablespaces

Tablespace, undo tablespace, and NDB logfile group statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `ALTER TABLESPACE` | ❌ | General tablespace alterations and diagnostics |
| `ALTER UNDO TABLESPACE` | ❌ | Undo tablespace syntax from the MySQL parser |
| `CREATE LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `CREATE TABLESPACE` | ❌ | General and NDB tablespace syntax, diagnostics |
| `CREATE UNDO TABLESPACE` | ❌ | Undo tablespace syntax, diagnostics |
| `DROP LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `DROP TABLESPACE` | ❌ | Tablespace deletion syntax, diagnostics |
| `DROP UNDO TABLESPACE` | ❌ | Undo tablespace deletion syntax, diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
