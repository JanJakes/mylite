# SQL replication

Binary log, replica, source, and Group Replication statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `BINLOG` | ❌ | Base64 binary log event statement syntax diagnostics |
| `SHOW BINARY LOG STATUS` | 🟡 | Limited embedded placeholder row for current binary-log coordinates: `binlog.000001`, position `4`, empty database filters, and empty executed GTID set; no physical binary log, live source position, privilege filtering, or replication side effects |
| `SHOW BINARY LOGS` | 🟡 | Limited embedded placeholder listing one synthetic `binlog.000001` entry with file size `4` and `Encrypted = No`; no physical binary log files, log index, rotation, purge, encryption state, privilege filtering, or replication side effects |
| `SHOW BINLOG EVENTS` | 🟡 | Limited embedded placeholder with one synthetic `Format_desc` event and accepted `IN`, unsigned `FROM`, and unsigned `LIMIT` filtering over that event; no physical binary log files, live event stream, log rotation, GTIDs, privilege filtering, or replication side effects |
| `SHOW REPLICA STATUS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels; no `FOR CHANNEL`, channel state, relay logs, source connection, source topology, privilege filtering, or replication side effects |
| `SHOW REPLICAS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels; no registered replica rows, source topology, privilege filtering, or replication side effects |
| `SHOW RELAYLOG EVENTS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels and accepted no-row `IN`, integer `FROM`, integer `LIMIT`, and `FOR CHANNEL ''` forms; no live relay-log files, event rows, non-empty channels, source topology, privilege filtering, or replication side effects |
| `PURGE BINARY LOGS` | ❌ | Binary log purge syntax |
| `RESET BINARY LOGS AND GTIDS` | ❌ | Binary log and GTID reset syntax |
| `@@sql_log_bin` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `1`/`ON`; Boolean session `SET` forms are connection-local, global scalar scope is rejected as session-only, and `SHOW GLOBAL VARIABLES` omits the session-only variable. No binary log files, GTID behavior, replication side effects, privilege checks, persisted state, or Performance Schema variable tables |
| `@@log_bin` | 🟡 | Limited fixed global scalar value `1` and `SHOW VARIABLES` value `ON`; no binary log files, GTID recovery, startup options, replication side effects, or mutable state |
| `@@log_bin_basename` | 🟡 | Limited fixed global placeholder `binlog`; no configured data directory, binary log sequence, path expansion, rotation, or file creation |
| `@@log_bin_index` | 🟡 | Limited fixed global placeholder `binlog.index`; no binary log index file, path expansion, rotation, or file creation |
| `@@log_bin_trust_function_creators` | 🟡 | Limited fixed global `0` / `OFF` placeholder with MySQL-style deprecation warnings for scalar reads and exact no-op global `SET` forms; no stored-function trust behavior, trigger behavior, binary logging, privileges, or mutable state |
| `@@server_id` / `@@server_id_bits` / `@@server_uuid` | 🟡 | Limited fixed global identity placeholders; no configured replication identity, persisted server UUID, startup options, or mutable state |
| `@@sql_require_primary_key` | 🟡 | Limited handle-local session value with scalar reads, `SHOW VARIABLES`, session/local assignment, fixed global readback `0`, and primary-key DDL enforcement for current supported local table creation and table-change paths; no mutable shared global state, replicated primary-key policy, table import behavior, or replication applier privilege semantics |
| `@@sql_replica_skip_counter` | 🟡 | Limited read-only scalar value `0` for no-scope and `global` forms; `session` and `local` scopes are rejected as global-only; no mutable state, relay-log event skipping, `START REPLICA` effects, channels, or GTID restrictions; deprecated `@@sql_slave_skip_counter` alias support is tracked separately |
| `@@sql_slave_skip_counter` | 🟡 | Limited read-only deprecated alias for `@@sql_replica_skip_counter`; returns `0` for no-scope and `global` forms and emits deprecation warning `1287`; `session` and `local` scopes are rejected as global-only; no mutable state, relay-log event skipping, `START REPLICA` effects, channels, or GTID restrictions |
| `SET sql_log_bin` | 🟡 | Session value assignment and readback are supported as embedded no-op state; no binary logging toggle side effects, GTID restrictions, privilege checks, or persisted state |
| `CHANGE REPLICATION FILTER` | ❌ | Replication filter syntax, diagnostics |
| `CHANGE REPLICATION SOURCE TO` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no source connection/channel metadata, implicit commit, replication thread, or diagnostics beyond the no-op warning |
| `RESET REPLICA` | ❌ | Replica metadata reset syntax |
| `START REPLICA` | ❌ | Replica start variants |
| `STOP REPLICA` | ❌ | Replica stop syntax and channel handling |
| `START GROUP_REPLICATION` | ❌ | Group Replication start syntax and user credentials |
| `STOP GROUP_REPLICATION` | ❌ | Group Replication stop syntax |

[Back to compatibility overview](../../COMPATIBILITY.md)
