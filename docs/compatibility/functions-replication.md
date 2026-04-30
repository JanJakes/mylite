# Replication functions

Asynchronous connection failover, Group Replication, replica position, and GTID
wait functions.

| Function | Status | Notes |
| --- | --- | --- |
| `asynchronous_connection_failover_add_managed()` | ❌ | Add managed failover source |
| `asynchronous_connection_failover_add_source()` | ❌ | Add failover source |
| `asynchronous_connection_failover_delete_managed()` | ❌ | Delete managed failover source |
| `asynchronous_connection_failover_delete_source()` | ❌ | Delete failover source |
| `asynchronous_connection_failover_reset()` | ❌ | Reset failover settings |
| `group_replication_disable_member_action()` | ❌ | Disable member action for event specified |
| `group_replication_enable_member_action()` | ❌ | Enable member action for event specified |
| `group_replication_get_communication_protocol()` | ❌ | Get group protocol version |
| `group_replication_get_write_concurrency()` | ❌ | Get write concurrency |
| `group_replication_reset_member_actions()` | ❌ | Reset member actions |
| `group_replication_set_as_primary()` | ❌ | Make a specific group member the primary |
| `group_replication_set_communication_protocol()` | ❌ | Set group protocol version |
| `group_replication_set_write_concurrency()` | ❌ | Set write concurrency |
| `group_replication_switch_to_multi_primary_mode()` | ❌ | Switch group primary mode |
| `group_replication_switch_to_single_primary_mode()` | ❌ | Switch group primary mode |
| `MASTER_POS_WAIT()` | ❌ | Wait for replica position |
| `SOURCE_POS_WAIT()` | ❌ | Wait for replica position |
| `WAIT_FOR_EXECUTED_GTID_SET()` | ❌ | Wait until the given GTIDs have executed on the replica |

[Back to compatibility overview](../../COMPATIBILITY.md)
