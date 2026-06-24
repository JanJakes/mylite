# Baseline Replication Control Placeholders

This slice covers MySQL 8.4.9 replication-control statements that are
server-only in a traditional MySQL deployment but have no durable embedded
effect in MyLite:

- `BINLOG`
- `PURGE BINARY LOGS`
- `RESET BINARY LOGS AND GTIDS`
- `CHANGE REPLICATION FILTER`
- `RESET REPLICA`
- `START REPLICA`
- `STOP REPLICA`
- `START GROUP_REPLICATION`
- `STOP GROUP_REPLICATION`

MyLite does not write binary logs, keep GTID recovery state, configure replica
channels, start applier threads, or join Group Replication. These statements
are therefore accepted as embedded admin no-ops with the existing server-only
warning instead of mutating server state or failing applications that issue
defensive administration SQL.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/binlog.html
- https://dev.mysql.com/doc/refman/8.4/en/purge-binary-logs.html
- https://dev.mysql.com/doc/refman/8.4/en/reset-binary-logs-and-gtids.html
- https://dev.mysql.com/doc/refman/8.4/en/change-replication-filter.html
- https://dev.mysql.com/doc/refman/8.4/en/reset-replica.html
- https://dev.mysql.com/doc/refman/8.4/en/start-replica.html
- https://dev.mysql.com/doc/refman/8.4/en/stop-replica.html
- https://dev.mysql.com/doc/refman/8.4/en/start-group-replication.html
- https://dev.mysql.com/doc/refman/8.4/en/stop-group-replication.html

## MySQL 8.4.9 Observations

Runtime probes were executed against the local `mysql:8.4.9` container
`mylite-mysql-849`.

```sql
START REPLICA;
START REPLICA IO_THREAD, SQL_THREAD FOR CHANNEL '';
STOP REPLICA;
STOP REPLICA SQL_THREAD FOR CHANNEL '';
RESET REPLICA;
RESET REPLICA ALL FOR CHANNEL '';
CHANGE REPLICATION FILTER REPLICATE_DO_DB = (wp);
CHANGE REPLICATION FILTER REPLICATE_WILD_DO_TABLE = ('wp.%') FOR CHANNEL '';
START GROUP_REPLICATION;
START GROUP_REPLICATION USER='u', PASSWORD='p', DEFAULT_AUTH='mysql_native_password';
STOP GROUP_REPLICATION;
RESET BINARY LOGS AND GTIDS;
BINLOG 'AAAA';
PURGE BINARY LOGS BEFORE '2000-01-01 00:00:00';
START SLAVE;
STOP SLAVE;
CHANGE MASTER TO MASTER_HOST='h';
```

Observed behavior:

- `START REPLICA` and the thread/channel variant are accepted by the grammar and
  fail at runtime with `1200 / HY000` when the server is not configured as a
  replica.
- `STOP REPLICA`, `STOP REPLICA SQL_THREAD FOR CHANNEL ''`,
  `RESET REPLICA`, `RESET REPLICA ALL FOR CHANNEL ''`, and
  `CHANGE REPLICATION FILTER REPLICATE_DO_DB = (wp)` succeed in the controlled
  no-replication container.
- `CHANGE REPLICATION FILTER ... FOR CHANNEL ''` can fail with
  `1794 / HY000` when replica infrastructure is not initialized for that
  channel.
- `START GROUP_REPLICATION`, credentialed `START GROUP_REPLICATION`, and
  `STOP GROUP_REPLICATION` are accepted by the grammar and fail with
  `3092 / HY000` when Group Replication is not configured.
- `RESET BINARY LOGS AND GTIDS` and `PURGE BINARY LOGS BEFORE ...` succeed and
  mutate binary-log state on MySQL.
- `BINLOG 'AAAA'` is documented as internal-use binary-log replay syntax; the
  short invalid payload probe is a syntax error in the comparison runtime.
- Removed `START SLAVE`, `STOP SLAVE`, and `CHANGE MASTER TO ...` forms are
  syntax errors in MySQL 8.4.9 and are not added by this slice.

## Scope

MyLite accepts representative valid MySQL 8.4.9 replication-control syntax as
`MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT` after the normal parser fails:

- `BINLOG ...`
- `PURGE BINARY LOGS ...`
- `RESET BINARY LOGS AND GTIDS`
- `RESET REPLICA ...`
- `CHANGE REPLICATION FILTER ...`
- `START REPLICA ...`
- `STOP REPLICA ...`
- `START GROUP_REPLICATION ...`
- `STOP GROUP_REPLICATION`

The placeholder classifier is intentionally suffix-tolerant for these
server-only controls. It requires a single statement, balanced parentheses, and
a non-obviously-incomplete tail for the newly classified `CHANGE`, `START`, and
`STOP` forms, but it does not build replication option descriptors.

Runtime behavior is the existing admin no-op contract:

- execution succeeds;
- no columns and no rows are returned;
- affected rows is `0`;
- one warning is appended with code `1105`, SQLSTATE `HY000`, and message
  `MyLite accepted this server-only statement as an embedded no-op`;
- `ROW_COUNT()` reports `0`;
- user data, catalogs, variables, selected schema, transaction state, binary-log
  placeholders, GTID placeholders, and replica metadata placeholders remain
  unchanged.

MyLite intentionally does not emulate MySQL's implicit commits for these
placeholders. There is no server state to mutate, and committing an application
transaction would be a surprising side effect for an embedded compatibility
no-op.

## Out Of Scope

This slice does not implement:

- physical binary-log files, event replay, purge, rotation, or GTID reset;
- binary-log event decoding for `BINLOG`;
- replica-source metadata, relay logs, connection state, applier state, thread
  state, channel descriptors, or missing-channel diagnostics;
- Group Replication membership, plugin state, credentials, or stream protocol;
- privilege checks for replication or binary-log administration;
- server-side implicit commits;
- removed `START SLAVE`, `STOP SLAVE`, or `CHANGE MASTER TO` aliases.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned placeholder shape. The
normal Lemon grammar remains the authority for fully implemented SQL; these
forms are recognized by the post-failure placeholder classifier.

```text
admin_noop_statement:
    BINLOG ...
  | PURGE BINARY LOGS ...
  | RESET BINARY LOGS AND GTIDS
  | RESET REPLICA ...
  | CHANGE REPLICATION FILTER ...
  | START REPLICA ...
  | STOP REPLICA ...
  | START GROUP_REPLICATION ...
  | STOP GROUP_REPLICATION
```

## Runtime Architecture

No SQLite public extension API, MyLite catalog change, or SQLite fork hook is
needed. The statement is represented as a raw admin no-op AST node and routed
to the existing runtime admin placeholder executor. The executor writes only
handle-owned diagnostics/warnings and previous-row-count state; it does not
issue SQLite SQL or touch the file format.

## Tests

Focused coverage includes:

- parser acceptance for representative replication-control placeholder forms;
- parser rejection preservation for removed `START SLAVE`, `STOP SLAVE`, and
  `CHANGE MASTER TO ...` aliases;
- runtime success/no-row/warning behavior for representative replication
  controls;
- runtime preservation of user transactions through admin no-ops;
- compatibility documentation for the exact placeholder contract.

## Compatibility Status

This slice turns the replication-control placeholder baseline green for the
named no-op surface. The broader SQL replication family remains limited because
MyLite intentionally has no physical binary logs, replica channels, GTIDs, or
Group Replication runtime.
