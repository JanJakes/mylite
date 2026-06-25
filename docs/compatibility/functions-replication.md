# Replication functions

Asynchronous connection failover, Group Replication, replica position, and GTID
wait functions.

| Function | Status | Notes |
| --- | --- | --- |
| `asynchronous_connection_failover_add_managed()` | ⚪ | Embedded no-replication placeholder returns the MySQL 8.4.9 valid-call UDF success string; no persisted source list |
| `asynchronous_connection_failover_add_source()` | ⚪ | Embedded no-replication placeholder returns the MySQL 8.4.9 valid-call UDF success string; no persisted source list |
| `asynchronous_connection_failover_delete_managed()` | ⚪ | Embedded no-replication placeholder returns the MySQL 8.4.9 valid-call UDF success string; no persisted source list |
| `asynchronous_connection_failover_delete_source()` | ⚪ | Embedded no-replication placeholder returns the MySQL 8.4.9 valid-call UDF success string; no persisted source list |
| `asynchronous_connection_failover_reset()` | ⚪ | Embedded no-replication placeholder returns the MySQL 8.4.9 valid-call UDF success string |
| `group_replication_disable_member_action()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_enable_member_action()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_get_communication_protocol()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_get_write_concurrency()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_reset_member_actions()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_set_as_primary()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_set_communication_protocol()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_set_write_concurrency()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_switch_to_multi_primary_mode()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `group_replication_switch_to_single_primary_mode()` | ⚪ | Target-runtime absent in the pinned MySQL 8.4.9 server; MyLite mirrors `1046`/`1305` missing-function diagnostics |
| `MASTER_POS_WAIT()` | 🟡 | MySQL 8.4.9-shaped no-replica scalar placeholder returns `NULL`; no binary-log position state or actual wait |
| `SOURCE_POS_WAIT()` | 🟡 | MySQL 8.4.9-shaped no-replica scalar placeholder returns `NULL`; no binary-log position state or actual wait |
| `WAIT_FOR_EXECUTED_GTID_SET()` | ⚪ | Fixed `gtid_mode=OFF` embedded diagnostic `3062 / HY000`; no GTID execution set |

[Back to compatibility overview](../../COMPATIBILITY.md)
