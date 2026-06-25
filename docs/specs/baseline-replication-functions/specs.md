# Baseline Replication Functions

## Scope

This slice covers the MySQL 8.4.9 replication-function rows that are relevant to
MyLite's embedded, no-replication runtime:

- asynchronous connection failover UDFs
- position wait functions
- `WAIT_FOR_EXECUTED_GTID_SET()`
- Group Replication functions absent from the pinned target runtime

MyLite does not implement binary logs, replication channels, appliers, GTID
state, or Group Replication membership. The compatibility goal is therefore to
match the observable MySQL 8.4.9 surface for valid scalar calls in an
unconfigured server, while keeping those design limits explicit.

Official MySQL 8.4 reference pages used for the independently authored
specification:

- <https://dev.mysql.com/doc/refman/8.4/en/replication-functions-async-failover.html>
- <https://dev.mysql.com/doc/refman/8.4/en/replication-functions-synchronization.html>
- <https://dev.mysql.com/doc/refman/8.4/en/gtid-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/group-replication-functions.html>

Runtime expectations were verified against MySQL `8.4.9` with
`docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot
--batch --raw --skip-column-names`.

## Behavior

### Asynchronous failover UDFs

The following generic scalar functions are accepted with their documented
argument counts:

- `asynchronous_connection_failover_add_managed(channel, managed_type,
  managed_name, host, port, network_namespace, primary_weight,
  secondary_weight)`
- `asynchronous_connection_failover_add_source(channel, host, port,
  network_namespace, weight)`
- `asynchronous_connection_failover_delete_managed(channel, managed_name)`
- `asynchronous_connection_failover_delete_source(channel, host, port,
  network_namespace)`
- `asynchronous_connection_failover_reset()`

MySQL 8.4.9 returns a success string for valid calls on the default test server.
MyLite returns the same success-string shape and does not persist source-list
metadata, because it has no replication channels or applier state.

Wrong argument counts use MyLite's existing native-function argument-count
diagnostic (`1582 / 42000`).

### Position wait functions

`MASTER_POS_WAIT(log_name, log_pos[, timeout][, channel])` and
`SOURCE_POS_WAIT(log_name, log_pos[, timeout][, channel])` are accepted with
two to four arguments. In an unconfigured server, MySQL 8.4.9 returns `NULL`
because replica source state is unavailable; MyLite returns `NULL` for the same
embedded no-replica case.

The functions are scalar placeholders only. They do not wait, inspect binary
logs, or maintain channel position state.

### GTID wait function

`WAIT_FOR_EXECUTED_GTID_SET(gtid_set[, timeout])` is accepted with one or two
arguments. MyLite currently exposes fixed `gtid_mode=OFF`; MySQL 8.4.9 rejects
the wait in that mode with `3062 / HY000`. MyLite returns that diagnostic for
valid argument counts.

### Group Replication functions

The pinned MySQL 8.4.9 runtime does not expose these unqualified functions in a
selected schema, and returns routine-missing diagnostics:

- `group_replication_disable_member_action()`
- `group_replication_enable_member_action()`
- `group_replication_get_communication_protocol()`
- `group_replication_get_write_concurrency()`
- `group_replication_reset_member_actions()`
- `group_replication_set_as_primary()`
- `group_replication_set_communication_protocol()`
- `group_replication_set_write_concurrency()`
- `group_replication_switch_to_multi_primary_mode()`
- `group_replication_switch_to_single_primary_mode()`

MyLite mirrors that target-runtime absence. An unqualified call without a
selected database returns `1046 / 3D000`. An unqualified call with a selected
database returns `1305 / 42000` with `FUNCTION schema.name does not exist`.

## Parser And Runtime Design

No grammar change is needed. These names already parse as generic scalar
functions. The implementation adds generic-function classification, result
metadata, and scalar evaluation in the existing runtime path.

No SQLite fork hook is needed. The behavior is MyLite-owned compatibility
logic: fixed string, `NULL`, and diagnostic outcomes that do not require
SQLite storage or planner integration.

## Compatibility Limits

This slice does not implement:

- binary logs or relay logs
- source/replica channel state
- GTID execution sets
- actual waits or timeouts
- asynchronous failover source-list persistence
- Group Replication plugin state, member actions, consensus, or membership
- row-source expression support for these replication functions

Those gaps remain explicit embedded-design limits until MyLite grows real
replication state.
