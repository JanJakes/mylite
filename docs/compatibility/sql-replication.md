# SQL replication

Binary log, replica, source, and Group Replication statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `BINLOG` | ❌ | Base64 binary log event statement syntax diagnostics |
| `PURGE BINARY LOGS` | ❌ | Binary log purge syntax |
| `RESET BINARY LOGS AND GTIDS` | ❌ | Binary log and GTID reset syntax |
| `@@sql_log_bin` | 🟡 | Limited read-only scalar value `1` for no-scope, `session`, and `local` forms; no mutable session state, global scope, binary log files, GTID behavior, or replication side effects |
| `@@sql_require_primary_key` | 🟡 | Limited read-only scalar value `0` for no-scope, `session`, `local`, and `global` forms; no mutable state, replicated primary-key policy, table import behavior, or replication applier privilege semantics |
| `SET sql_log_bin` | ❌ | Session binary logging toggle and privilege semantics |
| `CHANGE REPLICATION FILTER` | ❌ | Replication filter syntax, diagnostics |
| `CHANGE REPLICATION SOURCE TO` | ❌ | Source connection/channel options and diagnostics |
| `RESET REPLICA` | ❌ | Replica metadata reset syntax |
| `START REPLICA` | ❌ | Replica start variants |
| `STOP REPLICA` | ❌ | Replica stop syntax and channel handling |
| `START GROUP_REPLICATION` | ❌ | Group Replication start syntax and user credentials |
| `STOP GROUP_REPLICATION` | ❌ | Group Replication stop syntax |

[Back to compatibility overview](../../COMPATIBILITY.md)
