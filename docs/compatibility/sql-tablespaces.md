# SQL tablespaces

Tablespace, undo tablespace, and NDB logfile group statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `ALTER TABLE ... DISCARD/IMPORT TABLESPACE` | ⚪ | Parsed as an unsupported utility statement returning `1064 / 42000`, including partition and subpartition file-operation variants; no physical tablespace file mutation |
| `ALTER TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical tablespace mutation, datafile handling, metadata, or persistence |
| `ALTER UNDO TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical undo tablespace state or datafile handling |
| `CREATE LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `CREATE TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical tablespace creation, datafile handling, metadata, or persistence |
| `CREATE UNDO TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical undo tablespace creation, datafile handling, metadata, or persistence |
| `DROP LOGFILE GROUP` | ❌ | NDB logfile group syntax, diagnostics |
| `DROP TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical tablespace deletion, datafile handling, metadata, or persistence |
| `DROP UNDO TABLESPACE` | ⚪ | Accepted as an embedded no-op with warning `1105`; no physical undo tablespace deletion, datafile handling, metadata, or persistence |

[Back to compatibility overview](../../COMPATIBILITY.md)
