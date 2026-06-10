# SQL foreign servers

Foreign server metadata statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER SERVER` | ⚪ | Parsed as a server-only administrative no-op with warning `1105`; no persisted `mysql.servers` row changes, FEDERATED connections, privileges, binary logging, or implicit-commit emulation |
| `CREATE SERVER` | ⚪ | Parsed as a server-only administrative no-op with warning `1105`; no persisted `mysql.servers` row changes, FEDERATED connections, privileges, binary logging, or implicit-commit emulation |
| `DROP SERVER` | ⚪ | Parsed as a server-only administrative no-op with warning `1105`; no persisted `mysql.servers` row changes, FEDERATED connections, privileges, binary logging, or implicit-commit emulation |

[Back to compatibility overview](../../COMPATIBILITY.md)
