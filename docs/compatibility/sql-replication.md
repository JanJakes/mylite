# SQL replication

Binary log, replica, source, and Group Replication statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `BINLOG` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no binary-log event decoding, replay, files, privileges, or replication side effects |
| `SHOW BINARY LOG STATUS` | 🟡 | Limited embedded placeholder row for current binary-log coordinates: `binlog.000001`, position `4`, empty database filters, and empty executed GTID set; no physical binary log, live source position, privilege filtering, or replication side effects |
| `SHOW BINARY LOGS` | 🟡 | Limited embedded placeholder listing one synthetic `binlog.000001` entry with file size `4` and `Encrypted = No`; no physical binary log files, log index, rotation, purge, encryption state, privilege filtering, or replication side effects |
| `SHOW BINLOG EVENTS` | 🟡 | Limited embedded placeholder with one synthetic `Format_desc` event and accepted `IN`, unsigned `FROM`, and unsigned `LIMIT` filtering over that event; no physical binary log files, live event stream, log rotation, GTIDs, privilege filtering, or replication side effects |
| `SHOW REPLICA STATUS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels; no `FOR CHANNEL`, channel state, relay logs, source connection, source topology, privilege filtering, or replication side effects |
| `SHOW REPLICAS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels; no registered replica rows, source topology, privilege filtering, or replication side effects |
| `SHOW RELAYLOG EVENTS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels and accepted no-row `IN`, integer `FROM`, integer `LIMIT`, and `FOR CHANNEL ''` forms; no live relay-log files, event rows, non-empty channels, source topology, privilege filtering, or replication side effects |
| `PURGE BINARY LOGS` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no binary-log purge, file removal, GTID effects, privileges, or replication side effects |
| `RESET BINARY LOGS AND GTIDS` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no binary-log reset, GTID reset, file mutation, privileges, or replication side effects |
| `@@sql_log_bin` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `1`/`ON`; Boolean session `SET` forms are connection-local, global scalar scope is rejected as session-only, and `SHOW GLOBAL VARIABLES` omits the session-only variable. No binary log files, GTID behavior, replication side effects, privilege checks, persisted state, or Performance Schema variable tables |
| `@@log_bin` | 🟡 | Limited fixed global scalar value `1` and `SHOW VARIABLES` value `ON`; no binary log files, GTID recovery, startup options, replication side effects, or mutable state |
| `@@log_bin_basename` | 🟡 | Limited fixed global placeholder `binlog`; no configured data directory, binary log sequence, path expansion, rotation, or file creation |
| `@@log_bin_index` | 🟡 | Limited fixed global placeholder `binlog.index`; no binary log index file, path expansion, rotation, or file creation |
| `@@log_bin_trust_function_creators` | 🟡 | Limited fixed global `0` / `OFF` placeholder with MySQL-style deprecation warnings for scalar reads and exact no-op global `SET` forms; no stored-function trust behavior, trigger behavior, binary logging, privileges, or mutable state |
| `@@binlog_*` baseline variables | ✅ | Default scalar/`SHOW VARIABLES` rows, scope diagnostics, read-only diagnostics, deprecation warnings, and exact/default no-op `SET` forms are supported for the documented binary-log system-variable baseline; no physical logs, event caches, encryption, compression, row-image behavior, GTID recovery, or mutable binlog state |
| `@@server_id` / `@@server_id_bits` / `@@server_uuid` | 🟡 | Limited fixed global identity placeholders; no configured replication identity, persisted server UUID, startup options, or mutable state |
| `@@sql_require_primary_key` | 🟡 | Limited handle-local session value with scalar reads, `SHOW VARIABLES`, session/local assignment, fixed global readback `0`, and primary-key DDL enforcement for current supported local table creation and table-change paths; no mutable shared global state, replicated primary-key policy, table import behavior, or replication applier privilege semantics |
| `@@replica_*` / `@@slave_*` applier tuning variables | ✅ | Fixed scalar/`SHOW VARIABLES` rows, global-only/read-only diagnostics, exact/default no-op `SET GLOBAL`, and MySQL-shaped deprecation warnings for `slave_*` aliases; no applier workers, channels, checkpointing, packet handling, error skipping, or mutable replication state |
| `@@sql_replica_skip_counter` | ✅ | Fixed embedded placeholder value `0` for scalar/`SHOW VARIABLES`, global-only diagnostics, and exact/default global no-op `SET`; no mutable state, relay-log event skipping, `START REPLICA` effects, channels, or GTID restrictions |
| `@@sql_slave_skip_counter` | ✅ | Deprecated alias for `@@sql_replica_skip_counter`; returns `0` for scalar/`SHOW VARIABLES`, emits deprecation warning `1287` for scalar reads and accepted alias `SET`, has global-only diagnostics, and accepts exact/default global no-op `SET`; no mutable state, relay-log event skipping, `START REPLICA` effects, channels, or GTID restrictions |
| `SET sql_log_bin` | 🟡 | Session value assignment and readback are supported as embedded no-op state; no binary logging toggle side effects, GTID restrictions, privilege checks, or persisted state |
| `CHANGE REPLICATION FILTER` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no filter storage, channel metadata, implicit commit, privileges, or replication side effects |
| `CHANGE REPLICATION SOURCE TO` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no source connection/channel metadata, implicit commit, replication thread, or diagnostics beyond the no-op warning |
| `RESET REPLICA` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no channel reset, relay-log mutation, implicit commit, privileges, or replication side effects |
| `START REPLICA` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no IO or SQL thread state, channels, relay-log skipping, credentials, privileges, or replication side effects |
| `STOP REPLICA` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no IO or SQL thread state, channel handling, implicit commit, privileges, or replication side effects |
| `START GROUP_REPLICATION` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no Group Replication plugin state, credentials, membership, privileges, or stream side effects |
| `STOP GROUP_REPLICATION` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no Group Replication plugin state, membership, privileges, or stream side effects |

[Back to compatibility overview](../../COMPATIBILITY.md)
