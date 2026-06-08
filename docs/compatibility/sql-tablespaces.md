# SQL tablespaces

Tablespace, undo tablespace, and NDB logfile group statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `ALTER TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical tablespace mutation, datafile handling, metadata, or persistence |
| `ALTER UNDO TABLESPACE` | ❌ | Undo tablespace syntax from the MySQL parser |
| `CREATE LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `CREATE TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical tablespace creation, datafile handling, metadata, or persistence |
| `CREATE UNDO TABLESPACE` | ❌ | Undo tablespace syntax, diagnostics |
| `DROP LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `DROP TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical tablespace deletion, datafile handling, metadata, or persistence |
| `DROP UNDO TABLESPACE` | ❌ | Undo tablespace deletion syntax, diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
