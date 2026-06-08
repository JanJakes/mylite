# SQL server administration

Server lifecycle, process control, instance administration, and embedded server-operation diagnostics.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER INSTANCE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no TLS/keyring/redo/log operation side effects |
| `BINLOG` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no binary-log file state or replication side effects |
| `CACHE INDEX` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no key-cache state |
| `CLONE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no local or remote clone side effects |
| `FLUSH` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no logs, privileges, caches, tables, hosts, optimizer costs, status, or user-resource side effects |
| `KILL` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no connection/query kill behavior |
| `LOAD INDEX INTO CACHE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no key-cache preloading |
| `PURGE BINARY LOGS` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no binary-log purge or GTID side effects |
| `RESET` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no server-state reset side effects |
| `RESET PERSIST` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no persisted system-variable file or mutable server-global state |
| `RESTART` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no server restart side effect |
| `SHUTDOWN` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no server shutdown side effect |

[Back to compatibility overview](../../COMPATIBILITY.md)
